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
  Isolate* isolate;
  Persistent<Function> callback;
  int length;
  char* src;
};

// called on the main thread to queue work safely on the loop
static void parse(thread_resource_t* tr, void* data, size_t size) {
  EvalWork* work = (EvalWork*)data;
  Local<Function> fn = *reinterpret_cast<v8::Local<Function>*>(
          const_cast<v8::Persistent<Function>*>(&(work->callback)));
  Isolate* isolate = work->isolate; 
  isolate->Enter();
  HandleScope scope(isolate);

  // call out cb
  int argc = 1;
  Handle<String> str = String::NewFromUtf8(isolate, work->src, String::NewStringType::kNormalString, work->length);
  Local<Value> argv[] = {str};
  Local<Object> recv = Object::New(isolate); 
  fn->Call(recv, argc, argv);

  // cleanup
  work->callback.Reset();
  free(work->src);
  free(work);
  isolate->Exit();
}

void StackEm(Isolate* isolate, void* data) {
  RegisterState regState;
  SampleInfo sample;
  void* buffer = malloc(2048);
  isolate->GetStackSample(regState, &buffer, 2, &sample);
  printf("frames: %d\n", sample.frames_count);
  printf("state: ");
  switch (sample.vm_state) {
    case StateTag::JS:
      printf("JS");
    break;
    case StateTag::GC:
      printf("GC");
    break;
    case StateTag::COMPILER:
      printf("COMPILER");
    break;
    case StateTag::OTHER:
      printf("OTHER");
    break;
    case StateTag::EXTERNAL:
      printf("EXTERNAL");
    break;
    case StateTag::IDLE:
      printf("IDLE");
    break;
  }
  printf("\n");
  Local<StackTrace> trace = StackTrace::CurrentStackTrace(isolate, 2); 
  for (int i = 0; i < trace->GetFrameCount(); i++) {
    Local<StackFrame> frame = trace->GetFrame(i);
    Local<String> script = frame->GetScriptName();
    String::Utf8Value utf8(script);
    printf("stack [%d] %d:%d of %s\n", i, frame->GetLineNumber(), frame->GetColumn(), *utf8);
  }

  
}

// used on the thread to perform some work
// currently: printf the str passed in, send it back to main
//   via parse
static void test_cb(thread_resource_t* tr, void* data, size_t size) {
  EvalWork* work = (EvalWork*)data;
  // send same ptr back to main thread
  printf("work.src %s\n", work->src);
  work->isolate->RequestInterrupt(StackEm, nullptr);
  threx::enqueue_cb(tr, parse, work); 
}

// JS binding to create the work plan
// DOES NOT EXECUTE WORK
void RunMe(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  EvalWork* work = new EvalWork();
  Handle<Function> fn = Handle<Function>::Cast(args[0]);
  work->callback.Reset(isolate, fn);
  work->isolate = fn->GetIsolate();
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
