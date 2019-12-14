/**
 * SerialCommand - A Wiring/Arduino library to tokenize and parse commands
 * received over a serial port.
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
#include "SerialCommand.h"

/**
 * Constructor makes sure some things are set.
 */
SerialCommand::SerialCommand()
        : commandList(NULL),
          commandCount(0),
          defaultHandler(NULL),
          term('\n'),           // default terminator for commands, newline character
          last(NULL) {
    strcpy(delim, " "); // strtok_r needs a null-terminated string
    clearBuffer();
}

/**
 * Adds a "command" and a handler function to the list of available commands.
 * This is used for matching a found token in the buffer, and gives the pointer
 * to the handler function to deal with it.
 */
void SerialCommand::addCommand(const char *command,
                               void (*func)(unsigned char register_number, unsigned char register_value)) {
#ifdef SERIALCOMMAND_DEBUG
    SerialUSB.print("Adding command (");
    SerialUSB.print(commandCount);
    SerialUSB.print("): ");
    SerialUSB.println(command);
#endif

    commandList = (SerialCommandCallback *) realloc(commandList, (commandCount + 1) * sizeof(SerialCommandCallback));
    strncpy(commandList[commandCount].command, command, SERIALCOMMAND_MAXCOMMANDLENGTH);
    commandList[commandCount].command_function = func;
    commandCount++;
}

/**
 * This sets up a handler to be called in the event that the receveived command string
 * isn't in the list of commands.
 */
void SerialCommand::setDefaultHandler(void (*function)(const char *)) {
    defaultHandler = function;
}


/**
 * This checks the Serial stream for characters, and assembles them into a buffer.
 * When the terminator character (default '\n') is seen, it starts parsing the
 * buffer for a prefix command, and calls handlers setup by addCommand() member
 */
void SerialCommand::readSerial() {
    while (SerialUSB.available() > 0) {
        char inChar = SerialUSB.read();   // Read single available character, there may be more waiting
#ifdef SERIALCOMMAND_DEBUG
        SerialUSB.print(inChar);   // Echo back to serial stream
#endif

        inChar = tolower(inChar);
        if (inChar == term) {     // Check for the terminator (default '\r') meaning end of command
#ifdef SERIALCOMMAND_DEBUG
            SerialUSB.print("Received: ");
            SerialUSB.println(buffer);
#endif

            char *command = strtok_r(buffer, delim, &last);   // Search for command at start of buffer
            if (command != NULL) {
                boolean matched = false;

                for (int i = 0; i < commandCount; i++) {
#ifdef SERIALCOMMAND_DEBUG
                    SerialUSB.print("Comparing [");
                    SerialUSB.print(command);
                    SerialUSB.print("] to [");
                    SerialUSB.print(commandList[i].command);
                    SerialUSB.println("]");
#endif

                    // Compare the found command against the list of known commands for a match
                    if (strncmp(command, commandList[i].command, SERIALCOMMAND_MAXCOMMANDLENGTH) == 0) {
#ifdef SERIALCOMMAND_DEBUG
                        SerialUSB.print("Matched Command: ");
                        SerialUSB.println(command);
#endif

                        // Execute the stored handler function for the command
                        unsigned char unused1 = 0;
                        unsigned char unused2 = 0;
                        (*commandList[i].command_function)(unused1, unused2);
                        matched = true;
                        break;
                    }
                }
                if (!matched && (defaultHandler != NULL)) {
                    (*defaultHandler)(command);
                }
            }
            clearBuffer();
        } else if (isprint(inChar)) {     // Only printable characters into the buffer
            if (bufPos < SERIALCOMMAND_BUFFER) {
                buffer[bufPos++] = inChar;  // Put character into buffer
                buffer[bufPos] = '\0';      // Null terminate
            } else {
#ifdef SERIALCOMMAND_DEBUG
                Serial.println("Line buffer is full - increase SERIALCOMMAND_BUFFER");
#endif
            }
        }
    }
}


/**
 * Clear the input buffer.
 */
void SerialCommand::clearBuffer() {
    buffer[0] = '\0';
    bufPos = 0;
}

/**
 * Retrieve the next token ("word" or "argument") from the command buffer.
 * Returns NULL if no more tokens exist.
 */
char *SerialCommand::next() {
    return strtok_r(NULL, delim, &last);
}

/**
 * Print the list of commands.
 */

void SerialCommand::printCommands() {
    for (int i = 0; i < commandCount; i++) {
        SerialUSB.println(commandList[i].command);
    }
}
