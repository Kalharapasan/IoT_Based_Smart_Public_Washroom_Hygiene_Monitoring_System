# IoT_Based_Smart_Public_Washroom_Hygiene_Monitoring_System

## Project Details

This project demonstrates an IoT-based smart public washroom hygiene monitoring system. It combines hardware and software components to monitor washroom conditions and present the data in a Flutter application connected to Firebase Realtime Database.

### What It Shows

- Real-time washroom occupancy monitoring
- Water level and gas level status tracking
- Total usage analytics and event history
- SMS status monitoring for alerts
- Settings for notifications and threshold control

### App Features

- Home dashboard: live occupancy, water level, gas level, total usage, and last SMS status
- Analytics screen: usage trend chart and peak usage summary
- History screen: event log with usage and water level snapshots
- Settings screen: notification toggle, water alert threshold, GSM alert number, and logout action

### Data Sources

- `Live_Status` for current sensor and alert values
- `History_Log` for stored event history

### Hardware and Software Stack

- Flutter mobile app with Material 3 UI
- Firebase Core and Firebase Realtime Database
- `fl_chart` for usage visualization
- IoT controller and sensor data feed from the washroom prototype

### System Flow

1. Sensors and controller hardware collect washroom status data.
2. The data is pushed to Firebase Realtime Database.
3. The Flutter app listens to live updates from `Live_Status`.
4. Historical records are loaded from `History_Log`.
5. The app displays current values, charts, and alert controls in real time.

### Main Components

- `main.dart`: app entry point and navigation shell
- Home page: live status cards for occupancy, water, gas, and SMS state
- Analytics page: usage chart and peak usage summary
- History page: event timeline with usage snapshots
- Settings page: alert preferences and threshold control

### Requirements

- Flutter SDK 3.10 or later
- Firebase project configured for Realtime Database
- Android Studio, VS Code, or another Flutter-supported editor
- A connected device or emulator for testing

### Run the App

1. Open the `app` folder in your Flutter environment.
2. Run `flutter pub get` to install dependencies.
3. Ensure Firebase configuration files are present in the Android and iOS platforms.
4. Start the app with `flutter run`.

### Notes

- The analytics screen uses a sample chart layout and can be replaced with live database-driven trend data.
- The settings screen currently provides a UI for alert controls; backend persistence can be added if needed.

### Media Gallery

<table>
	<tr>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.33%20AM.jpeg" width="140" alt="Washroom prototype 1"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.33%20AM%20(1).jpeg" width="140" alt="Washroom prototype 2"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.34%20AM.jpeg" width="140" alt="Washroom prototype 3"></td>
	</tr>
	<tr>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.34%20AM%20(1).jpeg" width="140" alt="Washroom prototype 4"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.34%20AM%20(2).jpeg" width="140" alt="Washroom prototype 5"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.35%20AM.jpeg" width="140" alt="Washroom prototype 6"></td>
	</tr>
	<tr>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.08.35%20AM%20(1).jpeg" width="140" alt="Washroom prototype 7"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.26.04%20AM.jpeg" width="140" alt="Washroom prototype 8"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.26.08%20AM.jpeg" width="140" alt="Washroom prototype 9"></td>
	</tr>
	<tr>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.26.09%20AM.jpeg" width="140" alt="Washroom prototype 10"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.26.10%20AM.jpeg" width="140" alt="Washroom prototype 11"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.30.36%20AM.jpeg" width="140" alt="Washroom prototype 12"></td>
	</tr>
	<tr>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.30.49%20AM.jpeg" width="140" alt="Washroom prototype 13"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.30.58%20AM.jpeg" width="140" alt="Washroom prototype 14"></td>
		<td><img src="Image/WhatsApp%20Image%202026-04-06%20at%2012.38.53%20AM.jpeg" width="140" alt="Washroom prototype 15"></td>
	</tr>
</table>

### Video Preview

<video controls width="720" src="WhatsApp%20Video%202026-04-20%20at%207.19.20%20AM.mp4">
	Your browser does not support the video tag.
</video>

<video controls width="720" src="WhatsApp%20Video%202026-04-20%20at%2010.39.15%20AM.mp4">
	Your browser does not support the video tag.
</video>

[Open the first demo video](WhatsApp%20Video%202026-04-20%20at%207.19.20%20AM.mp4)

[Open the second demo video](WhatsApp%20Video%202026-04-20%20at%2010.39.15%20AM.mp4)
