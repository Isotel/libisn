# ISOTEL Sensor Network Library

## Overview

The [ISOTEL Sensor Network Protocol](https://www.isotel.eu/isn/overview.html) 
defines a set of simple re-usable protocol objects that can be used as stand-alone
or combined, on-demand, into a complex protocol structures.

This library provides [ISN Protocol Layer Stack implementation in C](https://www.isotel.eu/isn/), 
imitating object-like oriented programming. In this approach
individual layers may be arbitrarily stacked, chained, one with another to provide
desired protocol complexity. Structures can also be created on demand, dynamically.

Suggested reading:

- [ISOTEL Sensor Network Protocol Overview](https://www.isotel.eu/isn/overview.html) 
- \ref GR_ISN
- \ref GR_ISN_Message
- \ref GR_ISN_Frame
- \ref GR_ISN_User
- \ref GR_ISN_Dispatch
- \ref GR_ISN_Loopback

Cypress PSoC Support:

 - \ref GR_ISN_PSoC_UART standard PHY
 - \ref GR_ISN_PSoC_USBUART standard USB com port driver
 - \ref GR_ISN_PSoC_USBFS advanced driver
