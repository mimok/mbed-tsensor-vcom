# SSSCLI compatible firmware for TSENSOR and TSENSOR_DEV

## Introduction

This program allows to use the NXP SSSCLI command line application from Trust&Plug MiddleWare
to configure an SE050 chip.

## Installation

Compile and load this application into your board and use the SSSCLI command as explained
in NXP documentation. SSSCLI application is available in Plug & Trust
MiddleWare package available on [NXP website](https://www.nxp.com/products/security-and-authentication/authentication/edgelock-se050-plug-trust-secure-element-family-enhanced-iot-security-with-maximum-flexibility:SE050?&tab=Design_Tools_Tab).

## Usage with mbed-tsensor-lorawan
To retrieve the certificate corresponding to the private key which is used to sign the generated attestation
(located at address 0xF0000013) enter the following commands:

```bash
./ssscli connect se050 t1oi2c COM3
./ssscli get cert 0xF0000013 certificate.pem
./ssscli disconnect
```
More information on SE050 default content are available in [AN12436](https://www.nxp.com/docs/en/application-note/AN12436.pdf).
