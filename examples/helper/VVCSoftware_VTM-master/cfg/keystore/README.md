# Keystore

## Example CA and keys

This directory contains example CA and content provider keys and certificates. These example keys SHALL NOT be used in production environments.

Note that private keys should be kept secret.

| location| explanation |
| -------- | ------- |
| keystore/private/jvet_example_ca.key | JVET example CA private key. The used password is "example". |
| keystore/private/jvet_example_provider.key | JVET example content provider private key. There is no password protection for this key. |
| keystore/public/jvet_example_provider.crt | JVET example content provider public key certificate signed by example CA key|
| keystore/ca | Location for CA certificates. After adding new certificates, run `openssl rehash keystore/ca` to create hash based links. |
| keystore/ca/jvet_example_ca.crt | JVET example CA certificate |

## Creating certificates

### Creating a Certificate Authority (CA)

Note, that the following steps only illustrate creating an example CA.
For and actual CA it is of utmost importance to keep CA private keys secret, e.g. in offline storage.
Typically, CAs use multiple levels of intermediate signing certificates, which are used for everyday signing processes.

For this example, only one CA level is used.

For creating a CA, first a CA key need to be created. With OpenSSL, this can be done using the following command:

    openssl genrsa -out example_ca.key 4096

This creates a 4096 bit RSA key. In practical use, the key should be encrypted with a secure (long) passphrase, e.g. use

    openssl genrsa -aes256 -out example_ca.key 4096

to generate a key `example_ca.key` that is protected with AES encryption.

The create a self-signed certificate for the CA:

    openssl req -x509 -new -nodes -key example_ca.key -sha256 -days 1826 -out example_ca.crt

This will ask for Name, country, organization, etc. The days parameter indicates the number of days that the certificate will be valid.

### Creating a Content Provider Certificate

First create a key as for the CA:

    openssl genrsa -out example_content.key 4096

Create a signing request file:

    openssl req -new -key example_content.key -out example_content.csr

Sign the request with the CA key:

    openssl x509 -req -in example_content.csr -CA example_ca.crt -CAkey example_ca.key -out example_content.crt -days 730 -sha256

The days parameter indicates the number of days that the certificate will be valid.
