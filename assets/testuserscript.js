import * as std from 'std';
function test(danmaku) {
	std.puts("hello world");
	std.puts(danmaku);
}

globalThis["test"] = test;
