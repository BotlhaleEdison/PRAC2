/**
 ******************************************************************************
 * @file    task6_fsm.c
 * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
 *
 * Restructure the Task 4 transaction so the main loop never stops:
 *
 *   request -> write-enable -> write -> wait for EEPROM
 *           -> read-back -> verify -> result
 *
 * Rules (handout, Task 6):
 *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
 *     write cycle. You may use HAL_GetTick() to decide when the next status
 *     check is due.
 *   - Each call does a small amount of work, updates the state, and returns.
 *   - Do not put the whole transaction inside one blocking function called
 *     from the loop.
 *   - While a transaction is in progress the loop must still respond to PA3.
 *
 * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
 * (green) a successful verification, PB10 (red) a failed one.
 ******************************************************************************
 */
#include "prac2a.h"
/* TODO 6.1  Design your states. You need enough of them to tell apart at
 *           least: idle, write preparation, write transaction, EEPROM busy /
 *           status checking, read transaction, verification, and success or
 *           failure.
 */
typedef enum {
    EE_STATE_IDLE = 0,
    EE_STATE_WRITE_ENABLE,
    EE_STATE_WRITE_DATA,
    EE_STATE_WAIT_WRITE,
    EE_STATE_CHECK_STATUS,
    EE_STATE_READ_DATA,
    EE_STATE_VERIFY,
    EE_STATE_SUCCESS,
    EE_STATE_FAILURE
} ee_fsm_state_t;
volatile uint8_t ee_state = EE_STATE_IDLE;
volatile uint8_t ee_last_read = 0u;
volatile uint8_t ee_use_fsm = 1u;
/* Static state variables for polling timer and retry counter */
static uint32_t last_poll_time = 0u;
static uint32_t status_poll_count = 0u;
#define STATUS_POLL_INTERVAL_MS 1u
#define MAX_STATUS_POLLS 200u   /* Timeout if write exceeds ~200ms */
/* ==========================================================================
 * Non-blocking EEPROM write start
 * ========================================================================== */
void eeprom_write_byte_start(uint16_t address, uint8_t value)
{
    if (address >= EEPROM_SIZE_BYTES)
    {
        return;
    }
    eeprom_cs_low();
    spi_transfer(EEPROM_CMD_WRITE);
    spi_transfer((uint8_t)(address >> 8));
    spi_transfer((uint8_t)address);
    spi_transfer(value);
    eeprom_cs_high();
}
/* ==========================================================================
 * State Machine
 * ========================================================================== */
void update_eeprom_state_machine(uint32_t now)
{
    /* TODO 6.2  One step of your state machine. */
    /* Handle Abort Request (PA3) immediately across all active non-idle states */
    if (btn_abort_edge)
    {
        btn_abort_edge = 0u;
        btn_start_edge = 0u;
        /* Ensure SPI CS is left HIGH to avoid stuck bus transactions */
        eeprom_cs_high();
        eeprom_verify_ok = 0u;
        ee_state = EE_STATE_IDLE;
        return;
    }
    switch (ee_state)
    {
        case EE_STATE_IDLE:
            if (btn_start_edge)
            {
                btn_start_edge = 0u;
                eeprom_verify_ok = 0u;
                /* Start a new transaction */
                ee_state = EE_STATE_WRITE_ENABLE;
            }
            break;
        case EE_STATE_WRITE_ENABLE:
            /*
             * Send WREN as its own complete transaction.
             * The EEPROM's Write Enable Latch is set when CS rises.
             */
            eeprom_write_enable();
            ee_state = EE_STATE_WRITE_DATA;
            break;
        case EE_STATE_WRITE_DATA:
            /*
             * Send the WRITE command, address and data.
             *
             * IMPORTANT:
             * eeprom_write_byte() cannot be used here because Task 4's
             * version waits inside a blocking while-loop for the EEPROM
             * internal write cycle to finish.
             *
             * Task 6 only starts the write here. The FSM checks the EEPROM
             * status later in EE_STATE_CHECK_STATUS.
             */
            eeprom_write_byte_start(eeprom_test_addr, eeprom_test_byte);
            last_poll_time = now;
            status_poll_count = 0u;
            ee_state = EE_STATE_WAIT_WRITE;
            break;
        case EE_STATE_WAIT_WRITE:
            /*
             * Non-blocking delay check:
             * wait until the polling interval has elapsed before checking
             * the EEPROM status register.
             *
             * There is NO while-loop here.
             * The function returns to the main loop.
             */
            if ((uint32_t)(now - last_poll_time) >= STATUS_POLL_INTERVAL_MS)
            {
                ee_state = EE_STATE_CHECK_STATUS;
            }
            break;
        case EE_STATE_CHECK_STATUS:
        {
            /*
             * Read the EEPROM status register.
             *
             * Bit 0 is the Write-In-Progress (WIP) bit.
             */
            uint8_t status = eeprom_read_status();
            if ((status & 0x01u) == 0u)
            {
                /*
                 * EEPROM write complete.
                 */
                ee_state = EE_STATE_READ_DATA;
            }
            else
            {
                /*
                 * EEPROM is still busy.
                 */
                status_poll_count++;
                if (status_poll_count >= MAX_STATUS_POLLS)
                {
                    /*
                     * EEPROM write timeout failure.
                     */
                    eeprom_timeout_count++;
                    ee_state = EE_STATE_FAILURE;
                }
                else
                {
                    /*
                     * Still busy; schedule the next status poll.
                     */
                    last_poll_time = now;
                    ee_state = EE_STATE_WAIT_WRITE;
                }
            }
            break;
        }
        case EE_STATE_READ_DATA:
            /*
             * Read the byte back from the EEPROM.
             */
            ee_last_read = eeprom_read_byte(eeprom_test_addr);
            ee_state = EE_STATE_VERIFY;
            break;
        case EE_STATE_VERIFY:
            /*
             * Compare the byte read from EEPROM with the byte that
             * we originally wrote.
             */
            if (ee_last_read == eeprom_test_byte)
            {
                eeprom_verify_ok = 1u;
                ee_state = EE_STATE_SUCCESS;
            }
            else
            {
                eeprom_verify_ok = 0u;
                ee_state = EE_STATE_FAILURE;
            }
            break;
        case EE_STATE_SUCCESS:
            /*
             * A successful transaction has completed.
             *
             * Allow PA0 to start another transaction.
             */
            if (btn_start_edge)
            {
                btn_start_edge = 0u;
                eeprom_verify_ok = 0u;
                ee_state = EE_STATE_WRITE_ENABLE;
            }
            break;
        case EE_STATE_FAILURE:
            /*
             * A failed transaction has completed.
             *
             * Allow PA0 to start another transaction.
             */
            if (btn_start_edge)
            {
                btn_start_edge = 0u;
                eeprom_verify_ok = 0u;
                ee_state = EE_STATE_WRITE_ENABLE;
            }
            break;
        default:
            /*
             * Safety fallback if the state somehow becomes invalid.
             */
            ee_state = EE_STATE_IDLE;
            break;
    }
}
/* ==========================================================================
 * Outputs
 * ========================================================================== */
void update_outputs(void)
{
    /* TODO 6.3  PB0..PB7 show ee_last_read (leds_write_byte). Green after a
     *           successful verification, red after a failed one
     *           (status_leds_show). Decide what the status LEDs should show
     *           while a transaction is in progress, and after an abort.
     */
    /*
     * Update data LEDs PB0..PB7 with the last read byte.
     */
    leds_write_byte(ee_last_read);
    /*
     * Update status LEDs PB11 (Green) and PB10 (Red) based on state.
     */
    switch (ee_state)
    {
        case EE_STATE_IDLE:
            /*
             * Retain verification status from boot-time / previous run.
             */
            if (eeprom_verify_ok)
            {
                status_leds_show(STATUS_PASS);
            }
            else
            {
                status_leds_show(STATUS_OFF);
            }
            break;
        case EE_STATE_SUCCESS:
            /*
             * Green LED ON (PB11).
             */
            status_leds_show(STATUS_PASS);
            break;
        case EE_STATE_FAILURE:
            /*
             * Red LED ON (PB10).
             */
            status_leds_show(STATUS_FAIL);
            break;
        default:
            /*
             * While transaction is in progress or after an abort,
             * turn off status LEDs.
             */
            status_leds_show(STATUS_OFF);
            break;
    }
}
/* ==========================================================================
 * Setup
 * ========================================================================== */
void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    /*
     * Read-only boot-time path.
     *
     * This does not write to EEPROM. It allows the persistence test
     * to show the value that was already stored.
     */
    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;
    /*
     * TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test.
     */

    ee_state = EE_STATE_IDLE;
    ee_last_read = 0u;
    update_outputs();
}

/* ==========================================================================
 * Loop
 * ========================================================================== */
void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);
    if (ee_use_fsm)
    {
        /*
         * Perform exactly one small FSM step.
         *
         * The function returns after each state.
         */
        update_eeprom_state_machine(now);
        /*
         * Update LEDs after the state machine has progressed.
         */
        update_outputs();
    }
    else
    {
        /*
         * Optional Task 4 blocking path.
         *
         * This is NOT the non-blocking Task 6 FSM.
         */
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}
