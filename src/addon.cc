// addon.cc
#include <node.h>
#include <v8.h>
#include <threx.h>
#include <stdio.h>
#include <pthread.h>

using namespace v8;
using threx::thread_resource_t;
using threx::export_work;

struct EvalWork {
  Persistent<Function> callback;
  int length;
  char* src;
};

void parseCb(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("parseCb %d\n", tid);
  args.GetReturnValue().Set(args[0]);
}

static void parse(thread_resource_t* tr, void* data, size_t size) {
  Isolate* isolate = Isolate::GetCurrent();
  EvalWork* work = (EvalWork*)data;
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("parse %d\n", tid);
  Local<Function> fn = *reinterpret_cast<v8::Local<Function>*>(
          const_cast<v8::Persistent<Function>*>(&(work->callback)));
  work->callback.Reset();
  int argc = 1;
  Handle<String> str = String::NewFromUtf8(isolate, work->src, String::NewStringType::kNormalString, work->length);
  Local<Value> argv[] = {str};
  node::MakeCallback(isolate, v8::Object::New(isolate), fn, argc, argv);
}

static void test_cb(thread_resource_t* tr, void* data, size_t size) {
  EvalWork* work = (EvalWork*)data;
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("text_cb %d\n", tid);
  // send same ptr back to main thread
  threx::enqueue_cb(tr, parse, work); 
}

void RunMe(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Handle<Function> fn = Handle<Function>::Cast(args[1]);
  EvalWork* work = new EvalWork();
  work->callback.Reset(isolate, fn);
  work->src = "123";
  work->length = 3;
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("RunMe %d\n", tid);
  args.GetReturnValue().Set(export_work(isolate, test_cb));
}

void InitAll(Handle<Object> exports) {
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("InitAll %d\n", tid);
  NODE_SET_METHOD(exports, "eval", RunMe);
}

NODE_MODULE(worky, InitAll)
