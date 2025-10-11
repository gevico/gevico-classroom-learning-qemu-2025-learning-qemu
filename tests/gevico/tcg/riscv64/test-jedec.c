/*
 * Test G233 SPI JEDEC ID reading on G233 board
 *
 * Copyright (c) 2025 Zevorn(Chao Liu) chao.liu@yeah.net
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "crt.h"

#define G233_SPI0_BASE 0x10018000

/* Memory-mapped register access */
#define REG32(addr) (*(volatile uint32_t *)((uintptr_t)(G233_SPI0_BASE + (addr))))

/* G233 SPI register offsets */
#define SPI_CR1     0x00
#define SPI_CR2     0x04
#define SPI_SR      0x08
#define SPI_DR      0x0C
#define SPI_CSCTRL  0x10

/* SPI Control Register 1 (CR1) bits */
#define SPI_CR1_SPE     (1 << 6)   /* SPI Enable */
#define SPI_CR1_MSTR    (1 << 2)   /* Master mode */
#define SPI_CR1_BR_0    (1 << 3)   /* Baud rate control bit 0 */
#define SPI_CR1_BR_1    (1 << 4)   /* Baud rate control bit 1 */
#define SPI_CR1_BR_2    (1 << 5)   /* Baud rate control bit 2 */

/* SPI Status Register (SR) bits */
#define SPI_SR_TXE      (1 << 1)   /* Transmit buffer empty */
#define SPI_SR_RXNE     (1 << 0)   /* Receive buffer not empty */
#define SPI_SR_BSY      (1 << 7)   /* Busy flag */

/* CS Control Register bits */
#define SPI_CS_ENABLE   (1 << 0)   /* Enable CS0 */
#define SPI_CS_ACTIVE   (1 << 4)   /* Activate CS0 */

/* W25Q16 JEDEC ID commands */
#define W25Q16_CMD_JEDEC_ID 0x9F

static void spi_write_reg(uint32_t addr, uint32_t value)
{
    REG32(addr) = value;
}

static uint32_t spi_read_reg(uint32_t addr)
{
    return REG32(addr);
}

static void spi_wait_txe(void)
{
    uint32_t sr;
    do {
        sr = spi_read_reg(SPI_SR);
    } while (!(sr & SPI_SR_TXE));
}

static void spi_wait_rxne(void)
{
    uint32_t sr;
    do {
        sr = spi_read_reg(SPI_SR);
    } while (!(sr & SPI_SR_RXNE));
}

static void spi_wait_not_busy(void)
{
    uint32_t sr;
    do {
        sr = spi_read_reg(SPI_SR);
    } while (sr & SPI_SR_BSY);
}

static uint8_t spi_transfer_byte(uint8_t data)
{
    /* Wait for TX buffer to be empty */
    spi_wait_txe();
    
    /* Write data to DR register */
    spi_write_reg(SPI_DR, data);
    
    /* Wait for RX buffer to have data */
    spi_wait_rxne();
    
    /* Read received data */
    return spi_read_reg(SPI_DR) & 0xFF;
}

static void spi_cs_assert(void)
{
    /* Enable and activate CS0 */
    spi_write_reg(SPI_CSCTRL, SPI_CS_ENABLE | SPI_CS_ACTIVE);
}

static void spi_cs_deassert(void)
{
    /* Deactivate CS0 but keep it enabled */
    spi_write_reg(SPI_CSCTRL, SPI_CS_ENABLE);
}

static void spi_init(void)
{
    /* Reset SPI */
    spi_write_reg(SPI_CR1, 0x00000000);
    spi_write_reg(SPI_CSCTRL, 0x00000000);
    
    /* Configure SPI as master, enable SPI, set baud rate */
    spi_write_reg(SPI_CR1, SPI_CR1_MSTR | SPI_CR1_SPE | 
                  SPI_CR1_BR_2); /* Slow clock for testing */
    
    /* Wait for initialization */
    spi_wait_not_busy();
    
    /* Debug: Print SPI configuration */
    printf("SPI CR1: 0x%08X\n", spi_read_reg(SPI_CR1));
    printf("SPI SR: 0x%08X\n", spi_read_reg(SPI_SR));
}

static void test_jedec_id(void)
{
    uint8_t jedec_id[3];
    int i;
    
    printf("Testing G233 SPI JEDEC ID reading...\n");
    
    /* Initialize SPI */
    spi_init();
    
    /* Assert CS */
    printf("Asserting CS...\n");
    spi_cs_assert();
    printf("CSCTRL after assert: 0x%08X\n", spi_read_reg(SPI_CSCTRL));
    
    /* Send JEDEC ID command */
    printf("Sending JEDEC ID command 0x%02X\n", W25Q16_CMD_JEDEC_ID);
    spi_transfer_byte(W25Q16_CMD_JEDEC_ID);
    
    /* Read 3 bytes of JEDEC ID */
    for (i = 0; i < 3; i++) {
        printf("Reading JEDEC ID byte %d\n", i);
        jedec_id[i] = spi_transfer_byte(0x00);
        printf("Received byte %d: 0x%02X\n", i, jedec_id[i]);
    }
    
    /* Deassert CS */
    printf("Deasserting CS...\n");
    spi_cs_deassert();
    
    /* Print JEDEC ID */
    printf("JEDEC ID: 0x%02X 0x%02X 0x%02X\n", 
           jedec_id[0], jedec_id[1], jedec_id[2]);
    
    /* Check if we got valid JEDEC ID (W25X16 should be 0xEF 0x30 0x15) */
    crt_assert(jedec_id[0] == 0xEF && jedec_id[1] == 0x30 && jedec_id[2] == 0x15);
    printf("✓ JEDEC ID matches W25X16 (0xEF 0x30 0x15)\n");
}

int main(void)
{
    printf("G233 SPI JEDEC ID Test\n");
    printf("============================\n");
    
    test_jedec_id();
    
    printf("All tests passed!\n");
    return 0;
}
