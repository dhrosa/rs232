#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_DEBUG 1
#define CFG_TUD_ENABLED 1
  
#define CFG_TUD_CDC 2
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64

#define CFG_TUD_MSC 1
#define CFG_TUD_MSC_EP_BUFSIZE 4096

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_
