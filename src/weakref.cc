/*
 * Copyright (c) 2011, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <nan.h>

using namespace v8;

namespace {


class proxy_container {
public:
  Nan::Persistent<Object> proxy;
  Nan::Persistent<Object> emitter;
  Nan::Persistent<Object> target;
};


Nan::Persistent<ObjectTemplate> proxyClass;

Nan::Callback *globalCallback;

// Field index used to store the container object.
#define FIELD_INDEX_CONTAINER (0)

// Field index used to store the unique pointer value used to identify objects
// as instances created by this module).
#define FIELD_INDEX_IDENT (1)

// Count of internal fields used by instances created by this module.
#define FIELD_COUNT (2)

// The value stored at `FIELD_INDEX_IDENT`, which is designed to be unique to
// this module, barring intentional mischief from other native code.
#define IDENT_VALUE ((void *) &proxyClass)

bool IsDead(Local<Object> proxy) {
  assert(proxy->InternalFieldCount() == FIELD_COUNT);
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, FIELD_INDEX_CONTAINER)
  );
  return cont == NULL || cont->target.IsEmpty();
}


Local<Object> Unwrap(Local<Object> proxy) {
  assert(!IsDead(proxy));
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, FIELD_INDEX_CONTAINER)
  );
  Local<Object> _target = Nan::New<Object>(cont->target);
  return _target;
}

Local<Object> GetEmitter(Local<Object> proxy) {
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, FIELD_INDEX_CONTAINER)
  );
  assert(cont != NULL);
  Local<Object> _emitter = Nan::New<Object>(cont->emitter);
  return _emitter;
}


#define UNWRAP                            \
  Local<Object> obj;                      \
  const bool dead = IsDead(info.This());  \
  if (!dead) obj = Unwrap(info.This());   \

NAN_PROPERTY_GETTER(WeakNamedPropertyGetter) {
  UNWRAP
  info.GetReturnValue().Set(dead ? Local<Value>() : Nan::Get(obj, property).ToLocalChecked());
}


NAN_PROPERTY_SETTER(WeakNamedPropertySetter) {
  UNWRAP
  if (!dead) Nan::Set(obj, property, value);
  info.GetReturnValue().Set(value);
}


NAN_PROPERTY_QUERY(WeakNamedPropertyQuery) {
  info.GetReturnValue().Set(None);
}


NAN_PROPERTY_DELETER(WeakNamedPropertyDeleter) {
  UNWRAP
  info.GetReturnValue().Set(!dead && Nan::Delete(obj, property).FromJust());
}


NAN_INDEX_GETTER(WeakIndexedPropertyGetter) {
  UNWRAP
  info.GetReturnValue().Set(dead ? Local<Value>() : Nan::Get(obj, index).ToLocalChecked());
}


NAN_INDEX_SETTER(WeakIndexedPropertySetter) {
  UNWRAP
  if (!dead) Nan::Set(obj, index, value);
  info.GetReturnValue().Set(value);
}


NAN_INDEX_QUERY(WeakIndexedPropertyQuery) {
  info.GetReturnValue().Set(None);
}


NAN_INDEX_DELETER(WeakIndexedPropertyDeleter) {
  UNWRAP
  info.GetReturnValue().Set(!dead && Nan::Delete(obj, index).FromJust());
}

NAN_PROPERTY_ENUMERATOR(WeakNamedPropertyEnumerator) {
  UNWRAP
#if NODE_MAJOR_VERSION >= 7
  info.GetReturnValue().Set(dead ? Nan::New<Array>(0) : obj->GetPropertyNames(Nan::GetCurrentContext(), KeyCollectionMode::kIncludePrototypes, ONLY_ENUMERABLE, IndexFilter::kSkipIndices).ToLocalChecked());
#else
  info.GetReturnValue().Set(dead ? Nan::New<Array>(0) : Nan::GetPropertyNames(obj).ToLocalChecked());
#endif
}

NAN_INDEX_ENUMERATOR(WeakIndexedPropertyEnumerator) {
  UNWRAP
#if NODE_MAJOR_VERSION >= 7
  info.GetReturnValue().Set(dead ? Nan::New<Array>(0) : obj->GetPropertyNames(Nan::GetCurrentContext(), KeyCollectionMode::kIncludePrototypes, static_cast<PropertyFilter> (ONLY_ENUMERABLE | SKIP_STRINGS | SKIP_SYMBOLS), IndexFilter::kIncludeIndices).ToLocalChecked());
#else
  info.GetReturnValue().Set(dead ? Nan::New<Array>(0) : Nan::GetPropertyNames(obj).ToLocalChecked());
#endif
}

/**
 * Weakref callback function. Invokes the "global" callback function.
 */

static void TargetCallback(const Nan::WeakCallbackInfo<proxy_container> &info) {
  Nan::HandleScope scope;
  proxy_container *cont = info.GetParameter();

  // invoke global callback function
  Local<Value> argv[] = {
    Nan::New<Object>(cont->emitter)
  };
  Nan::Call(*globalCallback, 1, argv);

  // clean everything up
  Local<Object> proxy = Nan::New<Object>(cont->proxy);
  Nan::SetInternalFieldPointer(proxy, FIELD_INDEX_CONTAINER, NULL);
  cont->proxy.Reset();
  cont->emitter.Reset();
  delete cont;
}

/**
 * `_create(obj, emitter)` JS function.
 */

NAN_METHOD(Create) {
  if (!info[0]->IsObject()) return Nan::ThrowTypeError("Object expected");

  proxy_container *cont = new proxy_container();

  Local<Object> _target = info[0].As<Object>();
  Local<Object> _emitter = info[1].As<Object>();
  Local<Object> proxy = Nan::NewInstance(Nan::New<ObjectTemplate>(proxyClass)).ToLocalChecked();
  cont->proxy.Reset(proxy);
  cont->emitter.Reset(_emitter);
  cont->target.Reset(_target);
  Nan::SetInternalFieldPointer(proxy, FIELD_INDEX_CONTAINER, cont);
  Nan::SetInternalFieldPointer(proxy, FIELD_INDEX_IDENT, IDENT_VALUE);

  cont->target.SetWeak(cont, TargetCallback, Nan::WeakCallbackType::kParameter);

  info.GetReturnValue().Set(proxy);
}

bool isWeakRef (Local<Value> val) {
  if (!val->IsObject()) {
    return false;
  }

  Local<Object> obj = val.As<Object>();

  return obj->InternalFieldCount() == FIELD_COUNT
    && Nan::GetInternalFieldPointer(obj, FIELD_INDEX_IDENT) == IDENT_VALUE;
}

/**
 * `isWeakRef()` JS function.
 */

NAN_METHOD(IsWeakRef) {
  info.GetReturnValue().Set(isWeakRef(info[0]));
}

#define WEAKREF_FIRST_ARG                                                      \
  if (!isWeakRef(info[0])) {                                                   \
    return Nan::ThrowTypeError("Weakref instance expected");                   \
  }                                                                            \
  Local<Object> proxy = info[0].As<Object>();

/**
 * `get(weakref)` JS function.
 */

NAN_METHOD(Get) {
  WEAKREF_FIRST_ARG
  if (!IsDead(proxy))
  info.GetReturnValue().Set(Unwrap(proxy));
}

/**
 * `isDead(weakref)` JS function.
 */

NAN_METHOD(IsDead) {
  WEAKREF_FIRST_ARG
  info.GetReturnValue().Set(IsDead(proxy));
}

/**
 * `_getEmitter(weakref)` JS function.
 */

NAN_METHOD(GetEmitter) {
  WEAKREF_FIRST_ARG
  info.GetReturnValue().Set(GetEmitter(proxy));
}

/**
 * Sets the global weak callback function.
 */

NAN_METHOD(SetCallback) {
  Local<Function> callbackHandle = info[0].As<Function>();
  globalCallback = new Nan::Callback(callbackHandle);
}

/**
 * Init function.
 */

NAN_MODULE_INIT(Initialize) {
  Nan::HandleScope scope;

  Local<ObjectTemplate> p = Nan::New<ObjectTemplate>();
  proxyClass.Reset(p);
  Nan::SetNamedPropertyHandler(p,
                               WeakNamedPropertyGetter,
                               WeakNamedPropertySetter,
                               WeakNamedPropertyQuery,
                               WeakNamedPropertyDeleter,
                               WeakNamedPropertyEnumerator);
  Nan::SetIndexedPropertyHandler(p,
                                 WeakIndexedPropertyGetter,
                                 WeakIndexedPropertySetter,
                                 WeakIndexedPropertyQuery,
                                 WeakIndexedPropertyDeleter,
                                 WeakIndexedPropertyEnumerator);
  p->SetInternalFieldCount(FIELD_COUNT);

  Nan::SetMethod(target, "get", Get);
  Nan::SetMethod(target, "isWeakRef", IsWeakRef);
  Nan::SetMethod(target, "isDead", IsDead);
  Nan::SetMethod(target, "_create", Create);
  Nan::SetMethod(target, "_getEmitter", GetEmitter);
  Nan::SetMethod(target, "_setCallback", SetCallback);
}

} // anonymous namespace

NODE_MODULE(weakref, Initialize)
