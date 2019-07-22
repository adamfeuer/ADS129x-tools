/**
 * JsonCommand - A Wiring/Arduino library that uses JsonLines as
 * a protocol for sending commands and receiving data
 * over a serial port.
 *
 * Copyright (C) 2013-2019 Adam Feuer <adam@adamfeuer.com>
 * Copyright (C) 2012 Stefan Rado
 * Copyright (C) 2011 Steven Cogswell <steven.cogswell@gmail.com>
 *                    http://husks.wordpress.com
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
// #define JSONCOMMAND_DEBUG 1


#include "JsonCommand.h"

static char COMMAND_KEY[] = "COMMAND";
static char PARAMETERS_KEY[] = "PARAMETERS";

static char STATUS_CODE_KEY[] = "STATUS_CODE";
static char STATUS_TEXT_KEY[] = "STATUS_TEXT";
static char HEADERS_KEY[] = "HEADERS";
static char DATA_KEY[] = "DATA";

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
void JsonCommand::addCommand(const char *command, void (*function)()) {
  #ifdef JSONCOMMAND_DEBUG
    Serial.print("Adding command (");
    Serial.print(commandCount);
    Serial.print("): ");
    Serial.println(command);
  #endif

  commandList = (JsonCommandCallback *) realloc(commandList, (commandCount + 1) * sizeof(JsonCommandCallback));
  strncpy(commandList[commandCount].command, command, JSONCOMMAND_MAXCOMMANDLENGTH);
  commandList[commandCount].function = function;
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
void JsonCommand::setDefaultHandler(void (*function)(const char *)) {
  defaultHandler = function;
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
        Serial.print("Received: ");
        Serial.println(buffer);
      #endif
       //Serial.println("about to deserialize.");
       DeserializationError error = deserializeJson(json_command, buffer);

      if (error) {
        #ifdef JSONCOMMAND_DEBUG
          Serial.print(F("deserializeJson() failed: "));
	  Serial.println(error.c_str());
        #endif
        return;
      }

      const char* command = json_command[COMMAND_KEY];
      int command_num = find_command(command);
      #ifdef JSONCOMMAND_DEBUG
      // Serial.println(commandList[command_num].command);
      #endif
      // Execute the stored handler function for the command
      (*commandList[command_num].function)();
      //send_jsonlines_response(200, "Ok");

      clearBuffer();
    }
    else {
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

int JsonCommand::find_command(const char *command) {
  int result = -1;
  for (int i = 0; i < commandCount; i++) {
    if (strcmp(command, commandList[i].command) == 0) {
      result = i;
      break;
    }
  }
  return result;
}
 
void JsonCommand::send_jsonlines_response(int status_code, char *status_text) {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  root["STATUS_CODE"] = status_code;
  root["STATUS_TEXT"] = status_text;
  serializeJson(doc, SerialUSB);
  SerialUSB.println();
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

/**
 * Print the list of commands.
 */

void JsonCommand::printCommands() { 
   for (int i = 0; i < commandCount; i++) {
     SerialUSB.println(commandList[i].command);
   }
}
