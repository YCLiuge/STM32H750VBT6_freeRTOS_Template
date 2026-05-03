#include "lfs_port.h"
#include "w25q64.h"

#define LFS_READ_SIZE   256U
#define LFS_PROG_SIZE   256U
#define LFS_BLOCK_SIZE  4096U
#define LFS_BLOCK_COUNT ((W25Q64_CAPACITY / 2U) / LFS_BLOCK_SIZE)  /* 4 MB data area */

static int  lfs_read(const struct lfs_config *c, lfs_block_t block,
                     lfs_off_t off, void *buf, lfs_size_t size);
static int  lfs_prog(const struct lfs_config *c, lfs_block_t block,
                     lfs_off_t off, const void *buf, lfs_size_t size);
static int  lfs_erase(const struct lfs_config *c, lfs_block_t block);
static int  lfs_sync(const struct lfs_config *c);

static uint8_t g_lfs_read_buf[LFS_READ_SIZE];
static uint8_t g_lfs_prog_buf[LFS_PROG_SIZE];
static uint8_t g_lfs_lookahead_buf[32];

const struct lfs_config g_lfs_cfg =
{
  .read          = lfs_read,
  .prog          = lfs_prog,
  .erase         = lfs_erase,
  .sync          = lfs_sync,
  .read_size     = LFS_READ_SIZE,
  .prog_size     = LFS_PROG_SIZE,
  .block_size    = LFS_BLOCK_SIZE,
  .block_count   = LFS_BLOCK_COUNT,
  .cache_size    = LFS_READ_SIZE,
  .lookahead_size = sizeof(g_lfs_lookahead_buf),
  .block_cycles  = 500,
  .read_buffer   = g_lfs_read_buf,
  .prog_buffer   = g_lfs_prog_buf,
  .lookahead_buffer = g_lfs_lookahead_buf,
};

int lfs_port_init(void)
{
  return 0;
}

static int lfs_read(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, void *buf, lfs_size_t size)
{
  (void)c;
  uint32_t addr = (LFS_BLOCK_COUNT * LFS_BLOCK_SIZE) + (uint32_t)(block * LFS_BLOCK_SIZE + off);
  if (W25Q64_Read(addr, (uint8_t *)buf, size))
    return 0;
  return LFS_ERR_IO;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, const void *buf, lfs_size_t size)
{
  (void)c;
  uint32_t addr = (LFS_BLOCK_COUNT * LFS_BLOCK_SIZE) + (uint32_t)(block * LFS_BLOCK_SIZE + off);
  if (W25Q64_Write(addr, (const uint8_t *)buf, size))
    return 0;
  return LFS_ERR_IO;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
  (void)c;
  uint32_t addr = (LFS_BLOCK_COUNT * LFS_BLOCK_SIZE) + (uint32_t)(block * LFS_BLOCK_SIZE);
  if (W25Q64_EraseSector(addr))
    return 0;
  return LFS_ERR_IO;
}

static int lfs_sync(const struct lfs_config *c)
{
  (void)c;
  return 0;
}
