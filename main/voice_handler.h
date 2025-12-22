#ifndef VOICE_HANDLER_H
#define VOICE_HANDLER_H

// Parse Wit.ai response and execute commands
void parse_wit_response(char *json_str);

// Send audio to Wit.ai for speech recognition
void send_to_wit_ai(char *audio_data, int len);

#endif // VOICE_HANDLER_H
