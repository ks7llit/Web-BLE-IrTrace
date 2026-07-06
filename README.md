# Web BLE IrTrace

This branch starts the account-gated setup flow.

## Current structure

- `index.html` - sign in / create account entry page.
- `config.html` - current BLE configuration page.

## Current auth behavior

The first version uses browser `localStorage` only:

- sign up requires email, phone number, and password.
- password is salted and hashed in the browser.
- a local session unlocks `config.html`.
- signing out removes the local session.

This is useful for UI flow testing before hosting is active, but it is not production security. Once MilesWeb hosting is active, replace the local-only auth with server-side PHP/MySQL sessions and password hashing.

## Hosting notes

Web Bluetooth requires a secure origin. Deploy over HTTPS before testing BLE from the hosted page.
