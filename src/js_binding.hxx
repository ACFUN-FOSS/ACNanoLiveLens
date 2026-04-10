#ifndef NANOLIVELENS_JS_BINDING_HXX
#define NANOLIVELENS_JS_BINDING_HXX

metapp::MetaRepo &getGlobalMetaRepo();

void setupAllJsBinding(qjs::Context &ctx);
qjs::Value try_eval_module(std::string_view code);
qjs::Value try_eval(std::string_view code);

#endif // !Guard
