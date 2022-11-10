# ISOTEL Sensor Network Library

## Overview

The [ISOTEL Sensor Network Protocol](https://docs.isotel.org/isn/overview.html) 
defines a set of simple re-usable protocol objects that can be used as stand-alone
or combined, on-demand, into a complex protocol structures. 

Usage area includes:

- sensor communication
- control interfaces
- peer to peer communication for embedded system
- debugging and monitoring of a real-time systems

The [Isotel Device Manager](https://www.isotel.org/idm), a cross-platform Java application,
provides advanced integration of devices employing this protocol into the IoT by providing:

- driver-less host support from Linux, MAC and Win,
- python, and Jupyter integration, simplifies automation, analysis, and automated hardware testing
- data recorder with post-math processing for aggregated access among a set of data across numerous devices,
- syslog integration with severity reporting

## libisn Library

This library provides [ISN Protocol Layer Stack implementation in C](https://docs.isotel.org/isn/), 
imitating object-like oriented programming. In this approach
individual layers may be arbitrarily stacked, chained, one with another to provide
desired protocol complexity. Structures can also be created on demand, dynamically.

Suggested reading:

- [ISOTEL Sensor Network Protocol Overview](https://docs.isotel.org/isn/overview.html) 
- \ref GR_ISN
- \ref GR_ISN_Message
- \ref GR_ISN_Frame
- \ref GR_ISN_Frame_Long
- \ref GR_ISN_User
- \ref GR_ISN_Dispatch
- \ref GR_ISN_Redirect
- \ref GR_ISN_Dup

POSIX Support:

- \ref GR_ISN_UDP
- \ref GR_ISN_SERIAL

Cypress PSoC Support:

- \ref GR_ISN_PSoC_UART standard PHY
- \ref GR_ISN_PSoC_USBUART standard USB com port driver
- \ref GR_ISN_PSoC_USBFS advanced driver

Texas TM4C Support:

- \ref GR_ISN_TM4C_UART
