/**
 * JsonCommand - A Wiring/Arduino library that uses JsonLines as
 * a protocol for sending commands and receiving data
 * over a serial port.
 *
 * Copyright (C) 2013-2019 Adam Feuer <adam@adamfeuer.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

// uncomment for debugging on Serial interface (programming port)
// you must connect to Serial port first, then SerialUSB, since Serial will reset the Arduino Due
//#define JSONCOMMAND_DEBUG 1

#include "JsonCommand.h"

const char *COMMAND_KEY = "COMMAND";
const char *PARAMETERS_KEY = "PARAMETERS";
const char *STATUS_CODE_KEY = "STATUS_CODE";
const char *STATUS_TEXT_KEY = "STATUS_TEXT";
const char *HEADERS_KEY = "HEADERS";
const char *DATA_KEY = "DATA";

const char *MP_COMMAND_KEY = "C";
const char *MP_PARAMETERS_KEY = "P";
const char *MP_STATUS_CODE_KEY = "C";
const char *MP_STATUS_TEXT_KEY = "T";
const char *MP_HEADERS_KEY = "H";
const char *MP_DATA_KEY = "D";
/**
 * Constructor makes sure some things are set.
 */
JsonCommand::JsonCommand()
        : commandList(NULL),
          commandCount(0),
          defaultHandler(NULL),
          term('\n'),           // default terminator for commands, newline character
          last(NULL) {

    strcpy(delim, " "); // strtok_r needs a null-terminated string
    clearBuffer();
#ifdef JSONCOMMAND_DEBUG
    Serial.begin(BAUD_RATE); // for debugging
#endif
}

/**
 * Adds a "command" and a handler function to the list of available commands.
 * This is used for matching a found token in the buffer, and gives the pointer
 * to the handler function to deal with it.
 */
void JsonCommand::addCommand(const char *command,
                             void (*func)(unsigned char register_number, unsigned char register_value)) {
#ifdef JSONCOMMAND_DEBUG
    Serial.print("Adding command (");
    Serial.print(commandCount);
    Serial.print("): ");
    Serial.println(command);
#endif

    commandList = (JsonCommandCallback *) realloc(commandList, (commandCount + 1) * sizeof(JsonCommandCallback));
    strncpy(commandList[commandCount].command, command, JSONCOMMAND_MAXCOMMANDLENGTH);
    commandList[commandCount].command_function = func;
    commandCount++;

#ifdef JSONCOMMAND_DEBUG
    Serial.begin(BAUD_RATE); // for debugging
    Serial.println("Debug serial logging ready.");
#endif
}

/**
 * This sets up a handler to be called in the event that the receveived command string
 * isn't in the list of commands.
 */
void JsonCommand::setDefaultHandler(void (*func)(const char *)) {
    defaultHandler = func;
}


/**
 * This checks the Serial stream for characters, and assembles them into a buffer.
 * When the terminator character (default '\n') is seen, it starts parsing the
 * buffer as a JSON document (JSONLines).
 *
 * Then it checks to see if it has the proper structure as a command. If so,
 * it runs the command and outputs the result as a JSONLines document to the 
 * Serial stream.
 */
void JsonCommand::readSerial() {
    StaticJsonDocument<1024> json_command;
    while (SerialUSB.available() > 0) {
        char inChar = SerialUSB.read();   // Read single available character, there may be more waiting
#ifdef JSONCOMMAND_DEBUG
        Serial.print(inChar);   // Echo back to serial stream
#endif

        if (inChar == term) {     // Check for the terminator (default '\r') meaning end of command
#ifdef JSONCOMMAND_DEBUG
            Serial.println();
            Serial.print("Received: ");
            Serial.println(buffer);
#endif
            DeserializationError error = deserializeJson(json_command, buffer);

            if (error) {
#ifdef JSONCOMMAND_DEBUG
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
#endif
                clearBuffer();
                sendJsonLinesResponse(400, "Bad Request");
                return;
            }

            JsonObject command_object = json_command.as<JsonObject>();
            JsonVariant command_name_variant = command_object.getMember(COMMAND_KEY);
            if (command_name_variant.isNull()) {
#ifdef JSONCOMMAND_DEBUG
                Serial.println(F("Error: no command"));
#endif
                (*defaultHandler)("");
                clearBuffer();
                return;
            }
            const char *command = command_name_variant.as<const char *>();
            int command_num = findCommand(command);
            if (command_num < 0) {
                (*defaultHandler)(command);
                clearBuffer();
                return;
            }
#ifdef JSONCOMMAND_DEBUG
            Serial.println(commandList[command_num].command);
#endif
            JsonVariant parameters_variant = json_command.getMember(PARAMETERS_KEY);
            unsigned char register_number = 0;
            unsigned char register_value = 0;
            if (!parameters_variant.isNull()) {
                JsonArray params_array = parameters_variant.as<JsonArray>();
                size_t number_of_params = params_array.size();
#ifdef JSONCOMMAND_DEBUG
                Serial.println(number_of_params);
#endif
                if (number_of_params > 0) {
                    register_number = params_array[0];
                }
                if (number_of_params > 1) {
                    register_value = params_array[1];
                }
            }
            // Execute the stored handler function for the command
            (*commandList[command_num].command_function)(register_number, register_value);
            clearBuffer();
        } else {
            if (bufPos < JSONCOMMAND_BUFFER) {
                buffer[bufPos++] = inChar;  // Put character into buffer
                buffer[bufPos] = '\0';      // Null terminate
            } else {
#ifdef JSONCOMMAND_DEBUG
                Serial.println("Line buffer is full - increase JSONCOMMAND_BUFFER");
#endif
            }
        }
    }

}


int JsonCommand::findCommand(const char *command) {
    int result = -1;
    for (int i = 0; i < commandCount; i++) {
        if (strcmp(command, commandList[i].command) == 0) {
            result = i;
            break;
        }
    }
    return result;
}

void JsonCommand::sendJsonLinesResponse(int status_code, char *status_text) {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[STATUS_CODE_KEY] = status_code;
    root[STATUS_TEXT_KEY] = status_text;
    serializeJson(doc, SerialUSB);
    SerialUSB.println();
    doc.clear();
}

void JsonCommand::sendJsonLinesDocResponse(JsonDocument &doc) {
    serializeJson(doc, SerialUSB);
    SerialUSB.println();
    doc.clear();
}

void JsonCommand::sendMessagePackResponse(int status_code, char *status_text) {
    StaticJsonDocument<1024> doc;
    JsonObject root = doc.to<JsonObject>();
    root[MP_STATUS_CODE_KEY] = status_code;
    if (!status_text) {
        root[MP_STATUS_TEXT_KEY] = status_text;
    }
    serializeMsgPack(doc, SerialUSB);
    doc.clear();
}

void JsonCommand::sendMessagePackDocResponse(JsonDocument &doc) {
    serializeMsgPack(doc, SerialUSB);
    doc.clear();
}

/**
 * Clear the input buffer.
 */
void JsonCommand::clearBuffer() {
    buffer[0] = '\0';
    bufPos = 0;
}

/**
 * Retrieve the next token ("word" or "argument") from the command buffer.
 * Returns NULL if no more tokens exist.
 */
char *JsonCommand::next() {
    return strtok_r(NULL, delim, &last);
}

