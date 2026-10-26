# ESP32 for Deauthentication Frame Injection

In this project, the ESP32 acts both as a sniffer and spoofing device. By capturing management frames and saving MAC addresses, it can carry out a Denial-of-Service Attack by sending deauth frames globally or to a target. It exploits the absence of authentication in legacy WPA/WPA2 protocols. Modern WPA3 uses protected management frames. 
