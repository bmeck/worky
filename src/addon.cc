// addon.cc
#include <stdio.h>
#include <node.h>
#include <v8.h>
#include <threx.h>
#include <pthread.h>

using namespace v8;
using threx::thread_resource_t;
using threx::export_work;

// structure used for performing work on the thread
// needs a callback to let us get results from the thread
struct EvalWork {
  Persistent<Function> callback;
  int length;
  char* src;
};

// called on the main thread to queue work safely on the loop
static void parse(thread_resource_t* tr, void* data, size_t size) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("parse %d\n", tid);

  EvalWork* work = (EvalWork*)data;
  Local<Function> fn = *reinterpret_cast<v8::Local<Function>*>(
          const_cast<v8::Persistent<Function>*>(&(work->callback)));

  // cleanup
  work->callback.Reset();
  free(work->src);
  free(work);

  // call out cb
  int argc = 1;
  Handle<String> str = String::NewFromUtf8(isolate, work->src, String::NewStringType::kNormalString, work->length);
  Local<Value> argv[] = {str};
  node::MakeCallback(isolate, v8::Object::New(isolate), fn, argc, argv);
}

// used on the thread to perform some work
// currently: printf the str passed in, send it back to main
//   via parse
static void test_cb(thread_resource_t* tr, void* data, size_t size) {
  EvalWork* work = (EvalWork*)data;
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  // send same ptr back to main thread
  printf("work.src %s\n", work->src);
  threx::enqueue_cb(tr, parse, work); 
}

// JS binding to create the work plan
// DOES NOT EXECUTE WORK
void RunMe(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  EvalWork* work = new EvalWork();
  Handle<Function> fn = Handle<Function>::Cast(args[0]);
  work->callback.Reset(isolate, fn);
  String::Utf8Value utf(args[1]->ToString());
  work->length = utf.length();
  work->src = (char*)malloc(work->length);
  memcpy(work->src, *utf, work->length);

  // return our work and the external to pass it
  // external includes the cb for the main thread
  Handle<Array> arr = Array::New(isolate, 2);
  arr->Set(0, export_work(isolate, test_cb));
  arr->Set(1, External::New(isolate, reinterpret_cast<void*>(work)));
  args.GetReturnValue().Set(arr);
}

void InitAll(Handle<Object> exports) {
  NODE_SET_METHOD(exports, "eval", RunMe);
}

NODE_MODULE(worky, InitAll)
