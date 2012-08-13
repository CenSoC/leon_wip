/*

Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
(CenSoC), the University of Technology Sydney (UTS).

Copyright (c) 2012 The University of Technology Sydney (UTS), Australia
<www.uts.edu.au>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. All products which use or are derived from this software must display the
following acknowledgement:
  This product includes software developed by the Centre for the Study of
  Choice (CenSoC) at The University of Technology Sydney (UTS) and its
  contributors.
4. Neither the name of The University of Technology Sydney (UTS) nor the names
of the Centre for the Study of Choice (CenSoC) and contributors may be used to
endorse or promote products which use or are derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) AT THE
UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) AND CONTRIBUTORS ``AS IS'' AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) OR
THE UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

*/

/**
	@note -- a really really quick hack... take/use with a huuuge grain of salt...
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winuser.h>

#include <exception>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

// because my distro for mingw32 does not have symbolic constant (MOUSEEVENTF_VIRTUALDESK) defiend for the virutal desktop size...
#ifndef MOUSEEVENTF_VIRTUALDESK
#define MOUSEEVENTF_VIRTUALDESK 0x4000
#endif

// ditto
#ifndef MOUSEEVENTF_MOVE_NOCOALESCE
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#endif

int const static screen_width(::GetSystemMetrics(SM_CXVIRTUALSCREEN));
int const static screen_height(::GetSystemMetrics(SM_CYVIRTUALSCREEN));

double static screen_width_inverted;
double static screen_height_inverted;

void static
mouse_event(LONG x, LONG y, DWORD flags)
{
	INPUT mouse_input;
	mouse_input.type = INPUT_MOUSE; 
	mouse_input.mi.dx = x * screen_width_inverted + .5;
	if (mouse_input.mi.dx > 65534)
		mouse_input.mi.dx = 65534;
	mouse_input.mi.dy = y * screen_height_inverted + .5;
	if (mouse_input.mi.dy > 65534)
		mouse_input.mi.dy = 65534;
	mouse_input.mi.mouseData = 0;
	mouse_input.mi.dwFlags = flags;
	mouse_input.mi.time = 0;
	mouse_input.mi.dwExtraInfo = 0;
	if (::SendInput(1, &mouse_input, sizeof(mouse_input)) != 1)
		throw ::std::runtime_error("send mouse input call. x=(" + ::boost::lexical_cast< ::std::string>(x) + "), y=(" + ::boost::lexical_cast< ::std::string>(y) + "), button_down=(" + (flags & MOUSEEVENTF_LEFTDOWN ? "yes" : "no") + "), button_up=(" + (flags & MOUSEEVENTF_LEFTUP ? "yes" : "no") + ')');
}

void static
keyboard_event(WORD key, DWORD flags)
{
	INPUT keyboard_input;
	keyboard_input.type = INPUT_KEYBOARD;
	keyboard_input.ki.wVk = key;
	keyboard_input.ki.wScan = 0;
	keyboard_input.ki.dwFlags = flags;
	keyboard_input.ki.time = 0;
	keyboard_input.ki.dwExtraInfo = 0;
	if (::SendInput(1, &keyboard_input, sizeof(keyboard_input)) != 1)
		throw ::std::runtime_error("send keyboard input call. key=(" + ::boost::lexical_cast< ::std::string>(key) + "), key_down=(" + (flags & KEYEVENTF_KEYUP ? "yes" : "no") + ')');
}

void static
raise_error(::std::string const & what)
{
	::MessageBeep(MB_ICONEXCLAMATION);
	::MessageBox(NULL, what.c_str(), "FATAL ERROR AUTOCLICKER", MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
}

typedef ::boost::tokenizer< ::boost::escaped_list_separator<char> > tokenizer_type;

void static
mouse_move(::std::string & csv_token, tokenizer_type::const_iterator & i)
{
	csv_token = *++i;
	::boost::trim(csv_token);
	int const x(::boost::lexical_cast<unsigned>(csv_token));
	csv_token = *++i;
	::boost::trim(csv_token);
	int const y(::boost::lexical_cast<unsigned>(csv_token));
	mouse_event(x, y, MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE_NOCOALESCE);
}

POINT static cursor_pos; 
bool static cursor_pos_saved(false);
void static
restore_mouse_position()
{
	if (cursor_pos_saved == true)
		::SetCursorPos(cursor_pos.x, cursor_pos.y);
}

int
main(int const argc, char ** argv)
{
	try {
		if (argc != 2)
			throw ::std::runtime_error("the autoinput program needs exactly 1 argument=(filename of the actions .csv file, relative to current working directory)");

		if (!screen_width || !screen_height)
			throw ::std::runtime_error("GetSystemMetrics");

		screen_width_inverted = 65535. / screen_width;
		screen_height_inverted = 65535. / screen_height;
	 
		// save mouse position so it can be restored back to the location right before the automation process was started...
		if(!::GetCursorPos(&cursor_pos))
			throw ::std::runtime_error("GetCursorPos");
		else
			cursor_pos_saved = true;

		// open commands/actions csv file
		::std::ifstream actions_file(argv[1]);
		if (actions_file == false)
			throw ::std::runtime_error("cannot access file=(" + ::std::string(argv[1]) + ")");

		// parse actions file "line by line"
		::std::string action_line;
		::std::string csv_token;
		for (unsigned line_counter(1); ::std::getline(actions_file, action_line); ++line_counter) {
			if (action_line.empty() == false) { // skip empty lines (they are allowed)
				try {
					tokenizer_type tokens(action_line);
					tokenizer_type::const_iterator i(tokens.begin());
					csv_token = *i;
					::boost::trim(csv_token);
					if (csv_token.empty() == false) { // skip lines with the first cell empty -- denotes a comment 
						if (csv_token == "click") {
							mouse_move(csv_token, i);
							mouse_event(0, 0, MOUSEEVENTF_LEFTDOWN);
							mouse_event(0, 0, MOUSEEVENTF_LEFTUP);
						} else if (csv_token == "mouse_move") {
							mouse_move(csv_token, i);
						} else if (csv_token == "mouse_down") {
							mouse_event(0, 0, MOUSEEVENTF_LEFTDOWN);
						} else if (csv_token == "mouse_up") {
							mouse_event(0, 0, MOUSEEVENTF_LEFTUP);
						} else if (csv_token == "ascii") { // code expected to be in decimal... (until my version of boost is upgraded to parse hex notation as well... may be with numeric_cast...
							csv_token = *++i;
							::boost::trim(csv_token);
							unsigned const key(::boost::lexical_cast<unsigned>(csv_token));
							keyboard_event(key, 0);
							keyboard_event(key, KEYEVENTF_KEYUP);
						} else if (csv_token == "sleep") {
							csv_token = *++i;
							::boost::trim(csv_token);
							// later may need to write own busy-poll precise version (will wait till the feature is flaged as actually being needed)...
							::Sleep( ::boost::lexical_cast<unsigned>(csv_token));
						} else
							throw ::std::runtime_error("unrecognised action type=(" + csv_token + ") on line=(" + ::boost::lexical_cast< ::std::string>(line_counter) + ')');

						// syntax validity check: non-empty trailing arguments (cells) to any of the commands are not allowed
						while (++i != tokens.end()) {
							csv_token = *i;
							::boost::trim(csv_token);
							if (csv_token.empty() == false)
								throw ::std::runtime_error("command has too many arguments on line=(" + ::boost::lexical_cast< ::std::string>(line_counter) + ')');
						}

					}
				} catch (::boost::bad_lexical_cast const & e) {
					throw ::std::runtime_error("some numeric fields cannot be parsed, or are missing, on line=(" + ::boost::lexical_cast< ::std::string>(line_counter) + ')');
				}
			}
		}
		restore_mouse_position();
	} catch (::std::exception const & e) {
		restore_mouse_position();
		raise_error("ERROR=(" + ::std::string(e.what()) + ')');
	} catch (...) {
		restore_mouse_position();
		raise_error("COMPLETELY UNKONWN ERROR...");
	}
	return 0;
}

