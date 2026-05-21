JavaScript 绑定
****************

.. _cpp2js-rule:

C++ 值转换为 JavaScript 值的方式和规则
================================================

.. drawio-image:: c++_var_2_js_method.drawio

将 C++ 值转换为 JavaScript 值，有「翻译」「孪生」「移植」三种方式。ElementQuickJsBinding 具有通用的转换函数 ``cpp2Js`` 以及其 variant 版本 ``cppVariant2Js``，可将 C++ 值转换为 JavaScript 值。对于一个 C++ 值，ElementQuickJsBinding 会按照以下规则决定采用哪种方式：

* 对于任意 ``T``：

  * 对于数值类型（如 ``int``, ``float``, ``char`` 等），翻译为 JavaScript ``number``。
  * 对于 ``std::string``, ``char *``，翻译为 JavaScript ``string``。
  * 对于任何时间长度类型（如 ``std::chrono::duration`` 等），翻译为 JavaScript ``number``。
  * 对于任何时间点类型（如 ``std::chrono::time_point`` 等），翻译为 JavaScript ``Date``。
  * 对于一切容器，均翻译为 JavaScript ``Array``，其构成元素为 C++ 容器中的元素经过 ``cpp2Js`` 转换后的结果。
  * 对于用户自定义的结构体或类：

    * 如果该结构体或类是 *可翻译* 的，则翻译，否则移植。

* 对于除了 ``char *`` 以外的任意 ``T *`` 以及 ``std::reference_wrapper<T>``：均采用孪生方式。

转换方法：翻译（Translation）
-------------------------------------

翻译是将 C++ 值重构为一个语义等价的、但属于 JavaScript 类型的值。C++ 对象经过翻译后，在 JS 侧不再保留任何「C++ 身份」——它变成了一个纯粹的 JS 值。

「翻译」这个词的隐喻是：就像把中文翻译成英文，原文和译文是两种不同的东西，但表达的是同一个意思。如 C++ 的 std::chrono::milliseconds(500) 会翻译为 JS 的 500 —— 这是两个完全不同的对象，但它们表达的是同一个时间长度。

翻译主要用于纯数据类型，我们希望 a) 某种对象到 JavaScript 的转换不需要经由 EJSObj 这一中间介质；b) JavaScript JS 中有不同于 C++ 的表示。

具名要求: *可翻译*
------------------

一个 C++ 结构体或类是 *可翻译* 的，如果：

* 该结构体使用 regType 注册，且满足 ``EJS::Translatable`` 概念。若如此，ElementQuickJsBinding 在翻译此类型变量时将会利用指定的 ``translateToJS`` 函数，将这种 JavaScript 对象转换为 C++ 值时，会调用指定的 ``detranslate`` 来将 JavaScript 对象转换为 C++ 值。``detranslate`` 必须进行有效性校验，并在不符合要求时抛出异常。
* 该结构体使用 regType 注册，且 ``EJS::CompileTimeMeta<T>::autoTranslateViaReflectpp`` 为编译期真。若如此，ElementQuickJsBinding 在翻译此类型变量时将会利用 ``reflectpp`` 库转换为 rfl::Generic，然后转换为 JavaScript 对象；在将这种 JavaScript 对象转换为 C++ 值时，会利用 ``reflectpp`` 库将 JavaScript 对象转换为 rfl::Generic，然后反序列化为 C++ 值。
* 该结构体使用 regTypeDyn 注册，且提供了 translator。

转换方法：孪生（Twining）
------------------------------

「孪生」就如同「数字孪生」一词中的「孪生」。「孪生」是为一个 C++ 对象在 JS 侧创建一个代理分身，这个「代理分身」叫做「孪生体」。C++ 对象仍然存在于 C++ 侧，JS 侧的孪生体只是一个「遥控器」—— 它不是数据本身，而是持有对 C++ 对象的引用。

孪生体具有与原对象相同名称的代理属性。

什么情况下走孪生
~~~~~~~~~~~~~

- T * （除了 char * ，因为 char * 被翻译为 string ）
- std::reference_wrapper<T>
也就是说， 只要 C++ 侧表达的是"指向某对象的引用语义"，就走孪生 。


EJSObj
------------------

通过「移植」以及「孪生」转换出来的 JavaScript 对象，均为 “EJSObj”。EJSObj 是一种具有特定 opaque 的 JavaScript 对象，它的 opaque 中保存着关联的 C++ 对象的相关信息。

如果 EJSObj 是「移植」得来的，则其 opaque 中保存被移植的 C++ 对象本身。这是将源对象通过移动或复制构造实现的，见？？。如果 EJSObj 是「孪生」得来的，则其 opaque 中保存被孪生的 C++ 对象的引用，见？？。

被移植的 C++ 对象和被孪生的 C++ 对象统称为「关联的 C++ 对象」「关联对象」。

属性
~~~~~~~~~~~~~~~~~~~~~~

每当 JavaScript 代码尝试访问 EJSObj 的属性时，ElementQuickJsBinding 会获取关联 C++ 对象的属性值，然后转换为 JavaScript 类型并返回给 JavaScript 代码。

每当 JavaScript 代码尝试设置 EJSObj 的属性时（假设设置属性 prop 为 value），ElementQuickJsBinding 将首先将 value 转换为 C++ 类型，然后按照顺序查找遍历 prop 的所有 operator= 重载，直到找到一个可以将 value 转换为其参数的重载，最后调用该重载设置属性。

.. pcode::
   :linenos:

   \begin{algorithm}
   \begin{algorithmic}
      \STATE $cppValue = $ \CALL{jsToCpp}{$value$}
      \STATE $assignOps = $ $prop$ 的所有 operator= 重载
      \FOR{$assignOp$ in $assignOps$}
         \IF{$cppValue$ 可以转换为 $assignOp$ 的参数类型}
            \STATE $assignOp$($cppValue$)
         \ENDIF
      \ENDFOR
      \STATE \textbf{error}{找不到合适的 operator= 重载，无法设置属性 $prop$ 为 $cppValue$}
   \end{algorithmic}
   \end{algorithm}


返回值处理
~~~~~~~~~~~~~~~~~~~~~~

当 JavaScript 调用了 EJSObj 的函数，且函数有返回值，ElementQuickJsBinding 会先调用函数，然后如此处理返回值：

*  如果返回值为 ``T &``，则将之包装为 ``std::reference_wrapper<T>``，然后执行 ``cpp2VariantJs`` 转换，返回给 JavaScript 代码。
*  如果返回值为 ``T *`` 或 ``std::reference_wrapper<T>`` 或 ``T``，则直接执行 ``cpp2VariantJs`` 转换，返回给 JavaScript 代码。

有效性和生命周期
~~~~~~~~~~~~~~~~~~~~~~

如果通过 ``cpp2Js`` 转换且该结构体或类满足 ``EJS::LifetimeAware``，或通过 ``cppVariant2Js`` 转换并传入了 ``Rc<LifetimeInfo>`` （？），则该孪生体在被 JavaScript 使用时会判断源对象是否被销毁或移动。如果源对象被销毁或移动，则会抛出 JavaScript 异常，从而避免非法内存访问。

若非，则该孪生体在被 JavaScript 使用时不会判断源对象是否被销毁或移动，需要用户手动确保内存安全。

如果某个 C++ 对象的成员函数返回了一个引用或指针，该引用或指针将被孪生处理，且不满足概念「生命周期可感知」，则孪生的 EJSObj 的有效生命周期将会视为与这个 C++ 对象一致。

驱动程序（Driver）（未定）
---------------------------

ElementQuickJsBinding 提供一种机制，允许用户说明对于同一类类，均有哪些共同的函数或属性可供 JavaScript 侧调用。该功能在类模板上尤其有用。

理由：目前对任意类型 T 的转换方法是一套固定的规则（:ref:`cpp2js-rule`），我们希望用户可以针对某一类类（如 ``std::vector<T>`` 的所有实例）可以短路这种规则，自己编程实现到 JavaScript 的转换以及反向转换。

（？？这真的有意义吗？用户可以通过使 T 成为 *可翻译的* 来使 ElementQuickJsBinding 采用自定义的转换逻辑）

（有，因为可能 T 未必期望被翻译成 JavaScript 原生对象 / 值，而是一个有用户定义的 Opaque 的 JavaScript 对象）。

（？？那么使用驱动程序和采用固定规则的边界在哪里？要把所有转换规则都用驱动程序实现吗？）

（我们决定采用这样的方式组织代码，实现需要翻译的 C++ 类型的翻译，但不允许用户自行编写自己的驱动程序）

.. code-block:: C++
   :linenos:

   // https://en.cppreference.com/cpp/named_req/ContiguousContainer
   template <typename C>
   concept ContiguousContainer = requires(C c) {
      { c.data() } -> std::same_as<typename C::pointer>;
      { c.size() } -> std::convertible_to<std::size_t>;
      // Ensure elements are stored contiguously
      requires std::contiguous_iterator<typename C::iterator>;
   };

   template <ContiguousContainer T>
   class ContiguousContainerDriver
   {
      static std::optional<qjs::value> prototype;


      // For some classes...
      struct MyOpaque
      {
         gsl::not_null<T *> vec;
      };

      void setup() {
         prototype.emplace();
         *prototype["add"] = [](qjs::value &self, qjs::array args) {
            std::vector<metapp::Variant> cppArgs;
				for (auto &arg : args) {
					cppArgs.push_back(js2Cpp(arg));
				}

            ejsobjGetUserOpaque(self).vec->push_back(
               js2Cpp(cppArgs[0]).get<typename T::value_type&>()
            );
            return qjs::undefined();
         };
      }
      
      qjs::value cppToJs(T &vec) {
         qjs::value obj{ JS_NewObjectWithPrototype(*prototype) };
         ejsobjSetUserOpaque(MyOpaque{ &vec });
         return obj;
      }

      // For some classes...
      qjs::value cppToJs(T &vec) {
         qjs::array arr;
         for (auto &item : vec) {
            arr.push_back(cpp2Js(item));
         }
         return arr;
      }
   };

   regClass<std::vector<A>, ContiguousContainerDriver>(...);
   regClass<std::vector<B>, ContiguousContainerDriver>(...);

.. code-block:: C++
   :linenos:

   // （ElementQuickJsBinding 内部）
   qjs::value cppVariantToJs(metapp::variant &variant) {
      if (auto driverWrapperIt = drivers.find(variant.getMetaType())) {
         // drivers 是一个 MetaType -> Driver 类包装 的 Map

         // 包装后的 cppToJs 是 qjs::value cppToJs(metapp::variant &variant)
         driverWrapperIt->cppToJs(variant);
      } else {
         // 默認規則.....
      }
   }
