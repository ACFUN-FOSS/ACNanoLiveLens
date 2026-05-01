import * as std from 'std';
function test(danmaku) {
	std.puts("hello world");
	//std.puts(danmaku.age);
	std.puts(danmaku.foo());
}

globalThis["test"] = test;
