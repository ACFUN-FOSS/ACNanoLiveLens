# 风格规范
- 类、结构体、命名空间采取帕斯卡命名法（PascalCase）。函数、变量皆采取驼峰命名法（camelCase）。
- 类、结构体、命名空间的定义，开大括号位于新行。此外，各种语句（if, for, while 等等）后的开大括号、函数定义后的开大括号皆不换行。大括号初始化列表是一个例外，它根据美观考虑是否换行。举例：
    ```cpp
    class MyWidget
    {
        int foo;
    };

    void testFunc() {
        while (true) {
            if (true) {
            } else {
            }
            try {
            } catch (const std::exception& e) {
            }
        }
    }
    ```
- 初始化语法的使用规则：原则上只用等号初始化和大括号初始化。
    - 如果被初始化对象在语义上与用以初始化的值是相似的，使用等号初始化。如：
    ```cpp
    // 语义上表现：让 a 等于 10
    int a = 10;
    // 语义上表现：让 str 等于 "hello"
    std::string str = "hello";
    ```
    - 如果被初始化对象在语义上与用以初始化的值是不同的，需要表达出「初始化对象利用被初始化对象初始化，但初始化对象与被初始化对象是两种截然不同的事物」，或者需要用多个参数来初始化，建议使用大括号初始化。如：
    ```cpp
    // 语义上表现：MyWidget 是一个窗口类，而 "MainWindow" 是一个字符串，它们是两种截然不同的事物。
    MyWidget widget{ "MainWindow" };
    // 多个参数初始化 GameObject
    GameObject obj{ "GUIRoot", { 100, 152 } };
    ```

```