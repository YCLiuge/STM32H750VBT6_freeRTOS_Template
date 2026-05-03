#ifndef __LFS_PORT_H__
#define __LFS_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lfs.h"

extern const struct lfs_config g_lfs_cfg;

int lfs_port_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LFS_PORT_H__ */
