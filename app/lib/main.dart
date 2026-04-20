import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:fl_chart/fl_chart.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp();
  runApp(SmartWashroomPro());
}

class SmartWashroomPro extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorSchemeSeed: Colors.teal,
        brightness: Brightness.light,
      ),
      home: MainNavigation(),
    );
  }
}

class MainNavigation extends StatefulWidget {
  @override
  _MainNavigationState createState() => _MainNavigationState();
}

class _MainNavigationState extends State<MainNavigation> {
  int _selectedIndex = 0;
  final List<Widget> _pages = [
    HomePage(),
    AnalyticsPage(),
    HistoryPage(),
    SettingsPage(),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _pages[_selectedIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _selectedIndex,
        onDestinationSelected: (index) =>
            setState(() => _selectedIndex = index),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            selectedIcon: Icon(Icons.dashboard),
            label: 'Home',
          ),
          NavigationDestination(
            icon: Icon(Icons.analytics_outlined),
            selectedIcon: Icon(Icons.analytics),
            label: 'Analysis',
          ),
          NavigationDestination(
            icon: Icon(Icons.history_outlined),
            selectedIcon: Icon(Icons.history),
            label: 'History',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}

// --- 1. HOME PAGE (REAL-TIME) ---
class HomePage extends StatelessWidget {
  final DatabaseReference _dbRef = FirebaseDatabase.instance.ref().child(
    'Live_Status',
  );

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("System Live Status"),
        centerTitle: true,
      ),
      body: StreamBuilder(
        stream: _dbRef.onValue,
        builder: (context, snapshot) {
          if (!snapshot.hasData || snapshot.data!.snapshot.value == null) {
            return const Center(child: CircularProgressIndicator());
          }
          var data = snapshot.data!.snapshot.value as Map;
          return Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              children: [
                _buildStatusCard(
                  "Washroom Occupancy",
                  "${data['Occupancy']}",
                  Icons.person,
                  Colors.blue,
                ),
                const SizedBox(height: 10),
                Row(
                  children: [
                    Expanded(
                      child: _buildSmallCard(
                        "Water",
                        "${data['Water_Level']}L",
                        Icons.water_drop,
                        Colors.cyan,
                      ),
                    ),
                    Expanded(
                      child: _buildSmallCard(
                        "Gas",
                        "${data['Gas_Level']}",
                        Icons.air,
                        (data['Gas_Level'] > 2500) ? Colors.red : Colors.green,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 10),
                _buildStatusCard(
                  "Total Usage Today",
                  "${data['Total_Usage']}",
                  Icons.trending_up,
                  Colors.orange,
                ),
                const Spacer(),
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.teal.withValues(alpha: 0.1),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Row(
                    children: [
                      const Icon(Icons.info, color: Colors.teal),
                      const SizedBox(width: 10),
                      Text("Last SMS Status: ${data['SMS_Status']}"),
                    ],
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildStatusCard(
    String title,
    String value,
    IconData icon,
    Color color,
  ) {
    return Card(
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: color.withValues(alpha: 0.2),
          child: Icon(icon, color: color),
        ),
        title: Text(title, style: const TextStyle(fontWeight: FontWeight.bold)),
        trailing: Text(
          value,
          style: TextStyle(
            fontSize: 22,
            fontWeight: FontWeight.bold,
            color: color,
          ),
        ),
      ),
    );
  }

  Widget _buildSmallCard(
    String title,
    String value,
    IconData icon,
    Color color,
  ) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Icon(icon, color: color),
            const SizedBox(height: 5),
            Text(title, style: const TextStyle(fontSize: 12)),
            Text(
              value,
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
                color: color,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// --- 2. ANALYTICS PAGE ---
class AnalyticsPage extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Usage Analytics")),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            const Text(
              "Usage Frequency (Last 7 Events)",
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 20),
            SizedBox(
              height: 250,
              child: LineChart(
                LineChartData(
                  borderData: FlBorderData(show: true),
                  lineBarsData: [
                    LineChartBarData(
                      spots: [
                        const FlSpot(0, 1),
                        const FlSpot(1, 3),
                        const FlSpot(2, 2),
                        const FlSpot(3, 5),
                        const FlSpot(4, 3),
                        const FlSpot(5, 4),
                      ],
                      isCurved: true,
                      color: Colors.teal,
                      barWidth: 4,
                      belowBarData: BarAreaData(
                        show: true,
                        color: Colors.teal.withValues(alpha: 0.2),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 30),
            const Card(
              child: ListTile(
                leading: Icon(Icons.analytics, color: Colors.purple),
                title: Text("Peak Usage Time"),
                subtitle: Text("Mostly used between 08:00 AM - 10:00 AM"),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// --- 3. HISTORY PAGE ---
class HistoryPage extends StatelessWidget {
  final DatabaseReference _dbRef = FirebaseDatabase.instance.ref().child(
    'History_Log',
  );

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Activity History")),
      body: StreamBuilder(
        stream: _dbRef.onValue,
        builder: (context, snapshot) {
          if (!snapshot.hasData || snapshot.data!.snapshot.value == null) {
            return const Center(child: CircularProgressIndicator());
          }
          Map data = snapshot.data!.snapshot.value as Map;
          List items = data.values.toList().reversed.toList();

          return ListView.builder(
            itemCount: items.length,
            itemBuilder: (context, index) {
              var item = items[index];
              return Card(
                margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
                child: ListTile(
                  leading: const Icon(Icons.event_note, color: Colors.teal),
                  title: Text("${item['Event']}"),
                  subtitle: Text(
                    "Usage: ${item['Total_Usage']} | Water: ${item['Water_Level']}L",
                  ),
                  trailing: const Icon(Icons.chevron_right, size: 16),
                ),
              );
            },
          );
        },
      ),
    );
  }
}

// --- 4. SETTINGS PAGE ---
class SettingsPage extends StatefulWidget {
  @override
  _SettingsPageState createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  bool notifications = true;
  double waterThreshold = 100.0;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Settings")),
      body: ListView(
        children: [
          SwitchListTile(
            title: const Text("App Notifications"),
            subtitle: const Text("Get alerts on phone for critical events"),
            value: notifications,
            onChanged: (val) => setState(() => notifications = val),
          ),
          const Divider(),
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text("Water Alert Threshold: ${waterThreshold.toInt()}L"),
                Slider(
                  value: waterThreshold,
                  min: 50,
                  max: 400,
                  divisions: 7,
                  label: waterThreshold.round().toString(),
                  onChanged: (val) => setState(() => waterThreshold = val),
                ),
              ],
            ),
          ),
          const ListTile(
            leading: Icon(Icons.phone_android),
            title: Text("GSM Alert Number"),
            subtitle: Text("+94723466524"),
            trailing: Icon(Icons.edit, size: 18),
          ),
          const ListTile(
            leading: Icon(Icons.logout, color: Colors.red),
            title: Text("Logout", style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );
  }
}
