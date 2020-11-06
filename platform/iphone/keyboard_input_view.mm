/*************************************************************************/
/*  keyboard_input_view.mm                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#import "keyboard_input_view.h"

#include "core/os/keyboard.h"
#include "os_iphone.h"

@interface GodotKeyboardInputView () <UITextViewDelegate>

@end

@implementation GodotKeyboardInputView

- (instancetype)initWithCoder:(NSCoder *)coder {
	self = [super initWithCoder:coder];

	if (self) {
		[self godot_commonInit];
	}

	return self;
}

- (instancetype)initWithFrame:(CGRect)frame textContainer:(NSTextContainer *)textContainer {
	self = [super initWithFrame:frame textContainer:textContainer];

	if (self) {
		[self godot_commonInit];
	}

	return self;
}

- (void)godot_commonInit {
	self.hidden = YES;
	self.delegate = self;
}

- (void)dealloc {
	self.delegate = nil;
}

// MARK: Keyboard

- (BOOL)canBecomeFirstResponder {
	return YES;
}

- (BOOL)becomeFirstResponderWithString:(NSString *)existingString {
	self.text = existingString;
	return [self becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
	self.text = nil;
	return [super resignFirstResponder];
}

// MARK: Delegate

- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text {
	if (textView != self) {
		return NO;
	}

	if (text.length == 0) {
		for (int i = 0; i < range.length; i++) {
			OSIPhone::get_singleton()->key(KEY_BACKSPACE, true);
		}
	} else {
		String characters;
		characters.parse_utf8([text UTF8String]);

		for (int i = 0; i < characters.size(); i++) {
			int character = characters[i];
			OSIPhone::get_singleton()->key(character == 10 ? KEY_ENTER : character, true);
		}
	}

	return YES;
}

@end
