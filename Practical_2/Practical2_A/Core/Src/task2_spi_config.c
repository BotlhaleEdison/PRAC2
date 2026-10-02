/**
  ******************************************************************************
  * @file    task2_spi_config.c
  * @brief   TASK 2 : CONFIGURE THE HARDWARE SPI PERIPHERAL
  *
  * Configure the SPI peripheral entirely through its registers. Not allowed:
  * HAL_SPI_*, LL SPI transfer functions, CubeMX-generated SPI configuration,
  * GPIO bit-banging. Every register write you add should cite the RM0091
  * register it comes from.
  ******************************************************************************
  */

#include "prac2a.h"

volatile uint32_t dbg_gpiob_moder      = 0u;
volatile uint32_t dbg_gpiob_afrh       = 0u;
volatile uint32_t dbg_spi_cr1          = 0u;
volatile uint32_t dbg_spi_cr2          = 0u;
volatile uint32_t dbg_spi_sr           = 0u;
volatile uint32_t dbg_sck_hz_predicted = 0u;

void eeprom_spi_init(void)
{
    /* TODO 2.4  Enable the clocks this needs: the GPIO port that carries the
     *           pins AND the SPI peripheral itself. They are in different RCC
     *           enable registers - find both in RM0091. */
	 RCC-> AHBENR |= (1u<<18);
	 RCC-> APB1ENR |= (1u<<14);
    /* TODO 2.5  Chip select: make EE_PIN_CS a general purpose output driven
     *           HIGH. Think about the ORDER of those two steps. Be ready to
     *           explain why CS must start high. */
	 GPIOB->BSRR |= (1u<<12) ;

	 GPIOB->MODER &= ~(3u << 24);
	 GPIOB->MODER |=  (1u << 24);


      /* TODO 2.6  SCK, MISO and MOSI: put them in alternate-function mode
     *           (MODER), and select the alternate function number EE_SPI_AF
     *           (the AF register for pins 8..15). Both registers are needed. */
	 GPIOB->MODER &=  ~(3u << 26);
	 GPIOB->MODER &=  ~(3u << 28);
	 GPIOB->MODER &=  ~(3u << 30);
	 GPIOB->MODER |= (2u << 26);
	 GPIOB->MODER |= (2u << 28);
	 GPIOB->MODER |= (2u << 30);
	 /* Select AF0 for PB13, PB14, PB15 */

	 GPIOB->AFR[1] &= ~(0xFu << 20);
	 GPIOB->AFR[1] &= ~(0xFu << 24);
	 GPIOB->AFR[1] &= ~(0xFu << 28);
    /* TODO 2.7  Recommended: high output speed on SCK and MOSI, and a pull-up
     *           on MISO. In your report, explain what the EEPROM does with its
     *           output pin while CS is high, and why a pull-up helps. */
	 /* PB13 SCK = high speed */
	 GPIOB->OSPEEDR &= ~(3u << 26);
	 GPIOB->OSPEEDR |=  (3u << 26);

	 /* PB15 MOSI = high speed */
	 GPIOB->OSPEEDR &= ~(3u << 30);
	 GPIOB->OSPEEDR |=  (3u << 30);

	 /* PB14 MISO = pull-up */
	 GPIOB->PUPDR &= ~(3u << 28);
	 GPIOB->PUPDR |=  (1u << 28);


    /* TODO 2.8  SPI_CR2: 8-bit data frames, and a receive FIFO threshold that
     *           reports a received byte after 8 bits. Read the RM0091
     *           description of the FIFO threshold carefully - on the STM32F0
     *           the reset value does not suit single-byte transfers.
     *           Configure CR2 BEFORE enabling the peripheral. */

	 SPI2->CR2 &= ~(0xFu << 8);   // clear DS[3:0]
	 SPI2->CR2 |=  (7u << 8);     // DS = 0111 = 8-bit
	 /* RX FIFO threshold = 8-bit */
	 SPI2->CR2 |= (1u << 12);
    /* TODO 2.9  SPI_CR1: master mode, your baud-rate divider, the clock
     *           polarity and phase the EEPROM supports (EEPROM datasheet), and
     *           the bit order.
     *           You are driving CS yourself on a GPIO. Read RM0091 on
     *           slave-select (NSS) management in master mode: if the
     *           peripheral believes its NSS input is low it will leave master
     *           mode on its own, and you will see no clock at all. */

	 EE_SPI->CR1 &= ~(
	       (1u << 9)        // SSM
	     | (1u << 8)        // SSI
	     | (1u << 7)        // LSBFIRST
	     | (7u << 3)        // BR[2:0]
	     | (1u << 2)        // MSTR
	     | (1u << 1)        // CPOL
	     | (1u << 0)        // CPHA
	 );

	 EE_SPI->CR1 |= (1u << 9)             // SSM = 1
	              | (1u << 8)             // SSI = 1
	              | (EE_SPI_BR << 3)      // BR = 100 → /32
	              | (1u << 2);            // MSTR = 1

    /* Task 5 fault case. Leave this call exactly here: after your CR1 and CR2
     * configuration, before the peripheral is enabled. It does nothing unless
     * RUN_TASK is 5. */
    task5_fault_hook();

    /* TODO 2.10  Enable the peripheral. */
    EE_SPI->CR1 |= (1u << 6);

    dbg_gpiob_moder = EE_SPI_GPIO->MODER;
    dbg_gpiob_afrh = EE_SPI_GPIO->AFR[1];
    dbg_spi_cr1 = EE_SPI->CR1;
    dbg_spi_cr2 = EE_SPI->CR2;
    dbg_spi_sr = EE_SPI->SR;
    dbg_sck_hz_predicted = EE_SCK_HZ_PREDICTED;

    dbg_gpiob_moder      = EE_SPI_GPIO->MODER;
    dbg_gpiob_afrh       = EE_SPI_GPIO->AFR[1];
    dbg_spi_cr1          = EE_SPI->CR1;
    dbg_spi_cr2          = EE_SPI->CR2;
    dbg_spi_sr           = EE_SPI->SR;
    dbg_sck_hz_predicted = EE_SCK_HZ_PREDICTED;
}

/* ==========================================================================
 * RUN_TASK 2 - given
 * Configures SPI and then does nothing else, so you can inspect the selected
 * SPI peripheral and GPIOB in the SFR view (or the dbg_* variables) while the
 * program runs. PC13 keeps toggling so you can see the program has not hung.
 * ========================================================================== */

void task2_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
}

void task2_loop(uint32_t now)
{
    task1_gpio_update(now);
}
