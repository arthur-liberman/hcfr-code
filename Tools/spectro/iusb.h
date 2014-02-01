#ifndef _IUSB_H_

/* Standard USB protocol defines */

/*
 * Copyright 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* Device and/or Interface Class codes */
#define IUSB_CLASS_PER_INTERFACE     0x00    /* for DeviceClass */
#define IUSB_CLASS_AUDIO             0x01
#define IUSB_CLASS_COMM              0x02
#define IUSB_CLASS_HID               0x03
#define IUSB_CLASS_PHYSICAL          0x05
#define IUSB_CLASS_STILL_IMAGE       0x06
#define IUSB_CLASS_PRINTER           0x07
#define IUSB_CLASS_MASS_STORAGE      0x08
#define IUSB_CLASS_HUB               0x09
#define IUSB_CLASS_CDC_DATA          0x0a
#define IUSB_CLASS_SMART_CARD        0x0b
#define IUSB_CLASS_CONT_SECURITY     0x0d
#define IUSB_CLASS_VIDEO             0x0e 
#define IUSB_CLASS_DIAGNOSTIC        0xdc
#define IUSB_CLASS_WIRELESS          0xe0
#define IUSB_CLASS_MISC              0xef
#define IUSB_CLASS_APP_SPEC          0xfe
#define IUSB_CLASS_VENDOR_SPEC       0xff


/* Standard Descriptor types */
#define IUSB_DESC_TYPE_DEVICE                0x01
#define IUSB_DESC_TYPE_CONFIG                0x02
#define IUSB_DESC_TYPE_STRING                0x03
#define IUSB_DESC_TYPE_INTERFACE             0x04
#define IUSB_DESC_TYPE_ENDPOINT              0x05
#define IUSB_DESC_TYPE_DEVICE_QUALIFIER      0x06
#define IUSB_DESC_TYPE_OTHER_SPEED_CONFIG    0x07
#define IUSB_DESC_TYPE_INTERFACE_POWER       0x08
#define IUSB_DESC_TYPE_OTG                   0x09
#define IUSB_DESC_TYPE_DEBUG                 0x0a
#define IUSB_DESC_TYPE_INTERFACE_ASSOCIATION 0x0b

/* Descriptor sizes per descriptor type */
#define IUSB_DESC_TYPE_DEVICE_SIZE           18
#define IUSB_DESC_TYPE_CONFIG_SIZE           9
#define IUSB_DESC_TYPE_INTERFACE_SIZE        9
#define IUSB_DESC_TYPE_ENDPOINT_SIZE         7

/* Endpoint descriptor bEndpointAddress */
#define IUSB_ENDPOINT_NUM_MASK         0x0f
#define IUSB_ENDPOINT_DIR_MASK         0x80
#define IUSB_ENDPOINT_DIR_SHIFT        7
#define IUSB_ENDPOINT_IN               0x80
#define IUSB_ENDPOINT_OUT              0x00

/* Endpoint descriptor bmAttributes */
#define IUSB_ENDPOINT_TYPE_MASK        0x03
#define IUSB_ENDPOINT_TYPE_CONTROL     0x00
#define IUSB_ENDPOINT_TYPE_ISOCHRONOUS 0x01
#define IUSB_ENDPOINT_TYPE_BULK        0x02
#define IUSB_ENDPOINT_TYPE_INTERRUPT   0x03

#define IUSB_ENDPOINT_SYNC_MASK        0x0c
#define IUSB_ENDPOINT_SYNC_NONE        0x00
#define IUSB_ENDPOINT_SYNC_ASYNC       0x04
#define IUSB_ENDPOINT_SYNC_ADPT        0x08
#define IUSB_ENDPOINT_SYNC_SYNC        0x0c

#define IUSB_ENDPOINT_USAGE_MASK       0x30
#define IUSB_ENDPOINT_USAGE_DATA       0x00
#define IUSB_ENDPOINT_USAGE_FEED       0x10
#define IUSB_ENDPOINT_USAGE_IMPL_FEED  0x20

/* Endpoint descriptor wMaxPacketSize */
#define IUSB_ENDPOINT_MAX_PKTSZ_MASK   0x03ff
#define IUSB_ENDPOINT_MAX_XACTS_MASK   0x0c00
#define IUSB_ENDPOINT_MAX_XACTS_SHIFT  11

/* OTG descriptor bmAttributes */
#define IUSB_OTG_SRP                   0x00
#define IUSB_OTG_HNP                   0x01

/* Control request */
#define IUSB_REQ_HEADER_SIZE           8

/* Request bmRequestType */
#define IUSB_REQ_HOST_TO_DEV           0x00
#define IUSB_REQ_DEV_TO_HOST           0x80
#define IUSB_REQ_DIR_MASK              0x80

#define IUSB_REQ_TYPE_SHIFT           5
#define IUSB_REQ_TYPE_STANDARD        (0x00 << IUSB_REQ_TYPE_SHIFT)
#define IUSB_REQ_TYPE_CLASS           (0x01 << IUSB_REQ_TYPE_SHIFT)
#define IUSB_REQ_TYPE_VENDOR          (0x02 << IUSB_REQ_TYPE_SHIFT)
#define IUSB_REQ_TYPE_RESERVED        (0x03 << IUSB_REQ_TYPE_SHIFT)
#define IUSB_REQ_TYPE_MASK            (0x03 << IUSB_REQ_TYPE_SHIFT)

#define IUSB_REQ_RECIP_DEVICE          0x00
#define IUSB_REQ_RECIP_INTERFACE       0x01
#define IUSB_REQ_RECIP_ENDPOINT        0x02
#define IUSB_REQ_RECIP_OTHER           0x03
#define IUSB_REQ_RECIP_MASK            0x1f

/* Standard bRequest values */
#define IUSB_REQ_GET_STATUS            0x00
#define IUSB_REQ_CLEAR_FEATURE         0x01
#define IUSB_REQ_SET_FEATURE           0x03
#define IUSB_REQ_SET_ADDRESS           0x05
#define IUSB_REQ_GET_DESCRIPTOR        0x06
#define IUSB_REQ_SET_DESCRIPTOR        0x07
#define IUSB_REQ_GET_CONFIGURATION     0x08
#define IUSB_REQ_SET_CONFIGURATION     0x09
#define IUSB_REQ_GET_INTERFACE         0x0a
#define IUSB_REQ_SET_INTERFACE         0x0b
#define IUSB_REQ_SYNCH_FRAME           0x0c

/* Feature selector */
#define IUSB_FEATURE_EP_HALT              0
#define IUSB_FEATURE_DEV_REMOTE_WAKEUP    1

/* REQ_GET_STATUS return values */
#define IUSB_DEVICE_STATUS_SELFPWR        0x0001
#define IUSB_DEVICE_STATUS_REMOTE_WAKEUP  0x0002
#define IUSB_ENDPOINT_STATUS_HALT         0x0001

#define _IUSB_H_
#endif /* _IUSB_H_ */
