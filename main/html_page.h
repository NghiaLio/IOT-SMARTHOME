#pragma once

static const char *HTML_PAGE =
"<!DOCTYPE html>"
"<html>"
"<head><title>ESP32 Wi-Fi Setup</title></head>"
"<body style='font-family:sans-serif;text-align:center;margin-top:50px;'>"
"<h2>Configure Wi-Fi</h2>"
"<form action=\"/wifi\" method=\"post\">"
"SSID:<br><input type=\"text\" name=\"ssid\"><br><br>"
"Password:<br><input type=\"password\" name=\"pass\"><br><br>"
"Device Token:<br><input type=\"text\" name=\"token\" value=\"\"><br><br>"
"<input type=\"submit\" value=\"Save\">"
"</form>"
"</body></html>";
