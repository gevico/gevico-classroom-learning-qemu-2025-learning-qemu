#include "crt.h"

/* G233 SPI register definitions */
#define G233_SPI_BASE_ADDR 0x10018000

/* SPI Control Register 1 (SPI_CR1) */
#define SPI_CR1_OFFSET      0x00
#define SPI_CR1_CPHA        (1 << 0)   /* Clock Phase */
#define SPI_CR1_CPOL        (1 << 1)   /* Clock Polarity */
#define SPI_CR1_MSTR        (1 << 2)   /* Master Selection */
#define SPI_CR1_BR_SHIFT    3          /* Baud Rate Control shift */
#define SPI_CR1_SPE         (1 << 6)   /* SPI Enable */
#define SPI_CR1_LSBFIRST    (1 << 7)   /* Frame Format */
#define SPI_CR1_SSI         (1 << 8)   /* Internal Slave Select */
#define SPI_CR1_SSM         (1 << 9)   /* Software Slave Management */
#define SPI_CR1_RXONLY      (1 << 10)  /* Receive Only */
#define SPI_CR1_DFF         (1 << 11)  /* Data Frame Format */
#define SPI_CR1_CRCNEXT     (1 << 12)  /* CRC Transfer Next */
#define SPI_CR1_CRCEN       (1 << 13)  /* Hardware CRC Calculation Enable */
#define SPI_CR1_BIDIOE      (1 << 14)  /* Output Enable in Bidirectional Mode */
#define SPI_CR1_BIDIMODE    (1 << 15)  /* Bidirectional Data Mode Enable */

/* SPI Control Register 2 (SPI_CR2) */
#define SPI_CR2_OFFSET      0x04
#define SPI_CR2_TXEIE        (1 << 7)   /* TXE interrupt enable */
#define SPI_CR2_RXNEIE       (1 << 6)   /* RXNE interrupt enable */
#define SPI_CR2_ERRIE        (1 << 5)   /* Error interrupt enable */
#define SPI_CR2_SSOE         (1 << 4)   /* Software slave select output enable */


/* SPI Status Register (SPI_SR) */
#define SPI_SR_OFFSET       0x08
#define SPI_SR_RXNE         (1 << 0)   /* Receive buffer Not Empty */
#define SPI_SR_TXE          (1 << 1)   /* Transmit buffer Empty */
#define SPI_SR_CHSIDE       (1 << 2)   /* Channel side */
#define SPI_SR_UDR          (1 << 3)   /* Underrun flag */
#define SPI_SR_CRCERR       (1 << 4)   /* CRC error flag */
#define SPI_SR_MODF         (1 << 5)   /* Mode fault */
#define SPI_SR_OVR          (1 << 6)   /* Overrun flag */
#define SPI_SR_BSY          (1 << 7)   /* Busy flag */

/* SPI Data Register (SPI_DR) */
#define SPI_DR_OFFSET       0x0C

/* SPI CS Control Register (SPI_CSCTRL) */
#define SPI_CSCTRL_OFFSET   0x10
#define SPI_CSCTRL_CS0_EN   (1 << 0)   /* CS0 Enable */
#define SPI_CSCTRL_CS1_EN   (1 << 1)   /* CS1 Enable */
#define SPI_CSCTRL_CS2_EN   (1 << 2)   /* CS2 Enable */
#define SPI_CSCTRL_CS3_EN   (1 << 3)   /* CS3 Enable */
#define SPI_CSCTRL_CS0_ACT  (1 << 4)   /* CS0 Active */
#define SPI_CSCTRL_CS1_ACT  (1 << 5)   /* CS1 Active */
#define SPI_CSCTRL_CS2_ACT  (1 << 6)   /* CS2 Active */
#define SPI_CSCTRL_CS3_ACT  (1 << 7)   /* CS3 Active */

/* Memory-mapped register access */
#define REG32(addr) (*(volatile uint32_t *)(G233_SPI_BASE_ADDR + (addr)))

/* W25X16 Flash Commands */
#define W25X16_READ_DATA       0x03  /* Read Data */
#define W25X16_READ_STATUS     0x05  /* Read Status Register */
#define W25X16_WRITE_ENABLE    0x06  /* Write Enable */
#define W25X16_PAGE_PROGRAM    0x02  /* Page Program */
#define W25X16_SECTOR_ERASE    0x20  /* Sector Erase (4KB) */

static void g233_spi_init(void)
{
    /* Reset SPI */
    REG32(SPI_CR1_OFFSET) = 0;
    
    /* Configure SPI as master, 8-bit data, MSB first */
    uint32_t cr1 = SPI_CR1_MSTR |           /* Master mode */
                   (0x3 << SPI_CR1_BR_SHIFT) | /* Baud rate: fPCLK/16 */
                   SPI_CR1_SSM |            /* Software slave management */
                   SPI_CR1_SSI |            /* Internal slave select */
                   SPI_CR1_SPE;             /* Enable SPI */
    
    REG32(SPI_CR1_OFFSET) = cr1;
    
    printf("G233 SPI initialized\n");
}

static void g233_spi_cs_assert(void)
{
    /* Enable and activate CS0 */
    REG32(SPI_CSCTRL_OFFSET) = SPI_CSCTRL_CS0_EN | SPI_CSCTRL_CS0_ACT;
}

static void g233_spi_cs_deassert(void)
{
    /* Disable CS0 */
    REG32(SPI_CSCTRL_OFFSET) = 0;
}

static uint8_t g233_spi_transfer(uint8_t data)
{
    uint32_t sr;
    int retry_count = 0;
    
    /* Wait for TX buffer to be empty */
    do {
        sr = REG32(SPI_SR_OFFSET);
        retry_count++;
        if (retry_count > 1000) {
            printf("Warning: TX buffer empty timeout\n");
            break;
        }
    } while (!(sr & SPI_SR_TXE));
    
    /* Send data */
    REG32(SPI_DR_OFFSET) = data;
    
    /* Wait for RX buffer to have data */
    retry_count = 0;
    do {
        sr = REG32(SPI_SR_OFFSET);
        retry_count++;
        if (retry_count > 1000) {
            printf("Warning: RX buffer not empty timeout\n");
            break;
        }
    } while (!(sr & SPI_SR_RXNE));
    
    /* Return received data */
    return REG32(SPI_DR_OFFSET) & 0xFF;
}

static uint8_t flash_read_status(void)
{
    uint8_t cmd = W25X16_READ_STATUS;
    uint8_t status;
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    g233_spi_transfer(cmd);
    status = g233_spi_transfer(0x00);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    return status;
}

static void flash_write_enable(void)
{
    uint8_t cmd = W25X16_WRITE_ENABLE;
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    g233_spi_transfer(cmd);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
}

static void flash_wait_busy(void)
{
    uint8_t status;
    int retry_count = 0;
    
    do {
        status = flash_read_status();
        retry_count++;
        if (retry_count > 10000) {
            printf("Warning: Flash busy timeout\n");
            break;
        }
        for (volatile int i = 0; i < 1000; i++); /* Small delay */
    } while (status & 0x01); /* Check BUSY bit */
}

static void flash_sector_erase(uint32_t addr)
{
    uint8_t cmd = W25X16_SECTOR_ERASE;
    uint8_t addr_bytes[3];
    
    printf("Erasing sector at address 0x%06X...\n", addr);
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    /* Enable write */
    flash_write_enable();
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Send erase command and address */
    g233_spi_transfer(cmd);
    g233_spi_transfer(addr_bytes[0]);
    g233_spi_transfer(addr_bytes[1]);
    g233_spi_transfer(addr_bytes[2]);
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    /* Wait for erase to complete */
    flash_wait_busy();
    printf("Sector erase completed\n");
}

static void flash_page_program(uint32_t addr, const uint8_t *data, int len)
{
    uint8_t cmd = W25X16_PAGE_PROGRAM;
    uint8_t addr_bytes[3];
    
    printf("Programming page at address 0x%06X, length %d bytes...\n", addr, len);
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    /* Enable write */
    flash_write_enable();
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);
    
    /* Send program command and address */
    g233_spi_transfer(cmd);
    g233_spi_transfer(addr_bytes[0]);
    g233_spi_transfer(addr_bytes[1]);
    g233_spi_transfer(addr_bytes[2]);
    
    /* Send data */
    for (int i = 0; i < len; i++) {
        g233_spi_transfer(data[i]);
    }
    
    for (volatile int i = 0; i < 1000; i++);
    g233_spi_cs_deassert();
    
    /* Wait for program to complete */
    flash_wait_busy();
    printf("Page program completed\n");
}

static void flash_read_data(uint32_t addr, uint8_t *data, int len)
{
    uint8_t cmd = W25X16_READ_DATA;
    uint8_t addr_bytes[3];
    
    /* Convert address to bytes */
    addr_bytes[0] = (addr >> 16) & 0xFF;
    addr_bytes[1] = (addr >> 8) & 0xFF;
    addr_bytes[2] = addr & 0xFF;
    
    g233_spi_cs_assert();
    for (volatile int i = 0; i < 1000; i++);  /* Delay after CS assert */
    
    /* Send read command and address */
    g233_spi_transfer(cmd);
    g233_spi_transfer(addr_bytes[0]);
    g233_spi_transfer(addr_bytes[1]);
    g233_spi_transfer(addr_bytes[2]);
    
    /* Read data */
    for (int i = 0; i < len; i++) {
        data[i] = g233_spi_transfer(0x00);
    }
    
    for (volatile int i = 0; i < 1000; i++);  /* Delay before CS deassert */
    g233_spi_cs_deassert();
}

static void flash_write_test_data(void)
{
    uint8_t test_data[256] = {0}; /* One page of data */
    uint8_t read_data[256] = {0};
    int errors = 0;
    
    printf("=== Flash Write/Read Test ===\n");
    
    /* Generate test data: ASCII letters A-Z cycling */
    for (int i = 0; i < 256; i++) {
        test_data[i] = 'a' + (i % 26);
    }
    
    printf("Generated test data (first 32 bytes):\n");
    for (int i = 0; i < 32; i++) {
        printf("%c", test_data[i]);
    }
    printf("\n");
    
    /* Read status register */
    uint8_t status = flash_read_status();
    printf("Flash status before operations: 0x%02X\n", status);
    
    /* Erase first sector (4KB) */
    flash_sector_erase(0x000000);
    
    /* Program first page (256 bytes) */
    flash_page_program(0x000000, test_data, 256);
    
    /* Read back the data */
    printf("Reading back data from flash...\n");
    flash_read_data(0x000000, read_data, 256);
    
    /* Compare data */
    printf("Comparing written vs read data...\n");
    for (int i = 0; i < 256; i++) {
        if (test_data[i] != read_data[i]) {
            errors++;
            printf("Error at offset %d: expected 0x%02X ('%c'), got 0x%02X ('%c')\n",
                   i, test_data[i], test_data[i], read_data[i], read_data[i]);
        }
    }
    
    if (errors == 0) {
        printf("✓ SUCCESS: All 256 bytes match perfectly!\n");
    } else {
        printf("✗ FAILED: %d bytes don't match\n", errors);
        crt_assert(0);
    }
    
    /* Show first 32 bytes of read data */
    printf("Read data (first 32 bytes):\n");
    for (int i = 0; i < 32; i++) {
        printf("%c", read_data[i]);
    }
    printf("\n");
    
    /* Show hex dump of first 16 bytes */
    printf("Hex dump (first 16 bytes):\n");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", read_data[i]);
    }
    printf("\n");
}

int main(void)
{
    printf("G233 SPI Flash Write/Read Test\n");
    printf("===============================\n");
    
    g233_spi_init();
    flash_write_test_data();
    
    printf("Flash write/read test completed!\n");
    return 0;
}
