# nfc_keyboard_lock
Arduino project for electrical lock with keyboard and NFC/RFID-reader

Hardware:
  * Arduino UNO
  * MFRC522
  * Simple keypad 4x4
  * key or switch ('program key')
  * buzzer
  * 3 leds

We recommend to place arduino with opening relay inside room and
keypad with leds, buzzer and RFID reader outside.

Up to 14 cards are supported. To add card, press 'program' key,
then '*' on keyboard, bring the card, them press number,
corresponding to this card index.

To delete card, press 'program' key, then '#', then card index.

To delete ALL cards, press 'program', then '0'.

To write new open sequence, press 'program', then 'D',
then 4 keys of sequence (length can be changed in SEQ_LEN).

Card index can be 0-9 and A-D.

You can change values of secret_xxx variables to set sequences
for add/del cards without 'program' key. It is INSECURE and not
recommended.

Connections:
MFRC522:
 * NSS = 10
 * SCK = 13
 * MOSI = 11
 * MISO = 12

Keyboard:
 *  pins 2-9

 * A0 = open
 * A1 = blue LED
 * A2 = green LED
 * A3 = red LED
 * A4 = program
 * A5 = buzzer
