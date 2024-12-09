# Arduino Nano RP2040 Alarm Detection and Notification (for bread maker)

This project uses the Arduino Nano RP2040 Connect to detect the beep sound of a bread maker ([Panasonic SD-MDX4-K [Home Bakery Bistro]](https://panasonic.jp/bakery/products/SD-MDX4.html)) and send a notification via Pushover. It detects frequency & duration of alarm to only pick up an alarm from the machine.

For detailed instructions, code, and explanation, visit the [blog post](https://yoshidk6.hatenablog.com/entry/2024/12/09/100617).

## Key Features

- Detects specific audio frequencies using FFT.
- Analyzes beep duration to reduce false positives.
- Sends real-time notifications using the Pushover API.

## References

- Example of detecting sounds and sending notifications: [マイコン技術Navi: 【ESP32】特定の音が鳴ったらLINEで通知する（ご飯よ〜！を家族全員にお知らせシステム）](https://www.ekit-tech.com/?p=3575)
- FFT for analyzing beep sound: [Instructables](https://www.instructables.com/ApproxFFT-Fastest-FFT-Function-for-Arduino/)
- Get clock time: [DigiKey](https://www.digikey.com/en/maker/projects/how-to-build-an-rp2040-based-connected-clock-part-2/fcf43ab941e24b5f845b5d70b0f01f84)
