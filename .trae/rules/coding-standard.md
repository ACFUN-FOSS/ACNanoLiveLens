# 技术规范
- 本项目使用预编译头文件，只允许 include 项目内的头文件。任何第三方库、标准库头文件都统一放在对应的 stdafx.h 中。ElementQuickJsBinding 是一个例外。
- 所有文件已经通过编译器选项自动包含 stdafx.h，也请不要再显式包含 stdafx.h。
- 使用 hxx, cxx 文件名后缀。唯一的例外是 stdafx.h 和 stdafx.cpp，这是为了延续 VC++ 的习惯。

- Use reference when you can, use pointer when you must. 比如，对于引用语义的函数返回值和参数，请使用引用而不是指针，除非空值是可接受的。
- 如果 gsl::not_null<T*> 不期望在运行时改变，请使用 gsl::not_null<T const*>。

- （奉 komkoh 之名）采行 EATI C++。
    - 用 Box 和 Rc 来管理内存，而非 std::unique_ptr 和 std::shared_ptr。举例：
    ```cpp
    // 采取：
    auto window = newBox(MyWindow{ arg1, arg2, arg3 });
    // 而不是：
    auto window = std::make_unique<MyWindow>(arg1, arg2, arg3);
    ```
    - 需要在头文件中写出 Box 的类型名时应使用 ESSM::Box 而不是 Essentials::Memory::Box。
