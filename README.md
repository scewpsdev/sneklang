# sneklang
A simple C style systems programming language.
Originated out of frustration about C++'s design.
Compiles to LLVM with LLD for linking.

```D
// This is an example program.

import system.io.console;


const string LanguageName = "Sneklang";


func reverse(string[] messages) -> string[] {
	for (i, 0..messages.size / 2) {
		string tmp = messages[i];
		messages[i] = messages[messages.size - i - 1];
		messages[messages.size - i - 1] = tmp;
	}
	return messages;
}

public func main() {
	string[3] messages = { "!", LanguageName, "Hello " };
	console.writeln(reverse(messages));
}
```