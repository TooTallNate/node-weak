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
#include "node.h"
#include "nan.h"

using namespace v8;
using namespace node;

namespace {


class proxy_container {
public:
  Persistent<Object> proxy;
  Persistent<Object> target;
  Persistent<Object> emitter;
};


Persistent<ObjectTemplate> proxyClass;

NanCallback *globalCallback;


bool IsDead(Handle<Object> proxy) {
  assert(proxy->InternalFieldCount() == 1);
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    NanGetInternalFieldPointer(proxy, 0)
  );
  return cont == NULL || cont->target.IsEmpty();
}


Handle<Object> Unwrap(Handle<Object> proxy) {
  assert(!IsDead(proxy));
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    NanGetInternalFieldPointer(proxy, 0)
  );
  return NanPersistentToLocal(cont->target);
}

Handle<Object> GetEmitter(Handle<Object> proxy) {
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    NanGetInternalFieldPointer(proxy, 0)
  );
  assert(cont != NULL);
  return NanPersistentToLocal(cont->emitter);
}


#define UNWRAP                            \
  NanScope();                             \
  Handle<Object> obj;                     \
  const bool dead = IsDead(args.This());  \
  if (!dead) obj = Unwrap(args.This());   \


NAN_PROPERTY_GETTER(WeakNamedPropertyGetter) {
  UNWRAP
  NanReturnValue(dead ? Local<Value>() : obj->Get(property));
}


NAN_PROPERTY_SETTER(WeakNamedPropertySetter) {
  UNWRAP
  if (!dead) obj->Set(property, value);
  NanReturnValue(value);
}


NAN_PROPERTY_QUERY(WeakNamedPropertyQuery) {
  NanScope();
  NanReturnValue(Integer::New(None));
}


NAN_PROPERTY_DELETER(WeakNamedPropertyDeleter) {
  UNWRAP
  NanReturnValue(Boolean::New(!dead && obj->Delete(property)));
}


NAN_INDEX_GETTER(WeakIndexedPropertyGetter) {
  UNWRAP
  NanReturnValue(dead ? Local<Value>() : obj->Get(index));
}


NAN_INDEX_SETTER(WeakIndexedPropertySetter) {
  UNWRAP
  if (!dead) obj->Set(index, value);
  NanReturnValue(value);
}


NAN_INDEX_QUERY(WeakIndexedPropertyQuery) {
  NanScope();
  NanReturnValue(Integer::New(None));
}


NAN_INDEX_DELETER(WeakIndexedPropertyDeleter) {
  UNWRAP
  NanReturnValue(Boolean::New(!dead && obj->Delete(index)));
}


/**
 * Only one "enumerator" function needs to be defined. This function is used for
 * both the property and indexed enumerator functions.
 */

NAN_PROPERTY_ENUMERATOR(WeakPropertyEnumerator) {
  UNWRAP
  NanReturnValue(dead ? Array::New(0) : obj->GetPropertyNames());
}

/**
 * Weakref callback function. Invokes the "global" callback function.
 */

NAN_WEAK_CALLBACK(void *, TargetCallback) {
  NanScope();

  assert(NAN_WEAK_CALLBACK_OBJECT.IsNearDeath());
  void *arg = NAN_WEAK_CALLBACK_DATA(void *);
  proxy_container *cont = reinterpret_cast<proxy_container *>(arg);

  // invoke global callback function
  Local<Value> argv[] = {
    NanPersistentToLocal(NAN_WEAK_CALLBACK_OBJECT),
    NanPersistentToLocal(cont->emitter)
  };
  globalCallback->Call(2, argv);

  // clean everything up
  NanSetInternalFieldPointer(NanPersistentToLocal(cont->proxy), 0, NULL);
  NanDispose(cont->proxy);
  NanDispose(cont->target);
  NanDispose(cont->emitter);
  delete cont;
}

/**
 * `_create(obj, emitter)` JS function.
 */

NAN_METHOD(Create) {
  NanScope();
  if (!args[0]->IsObject()) return NanThrowTypeError("Object expected");

  proxy_container *cont = new proxy_container();

  Local<Object> proxy = NanPersistentToLocal(proxyClass)->NewInstance();
  NanAssignPersistent(Object, cont->proxy, proxy);
  NanAssignPersistent(Object, cont->target, args[0].As<Object>());
  NanAssignPersistent(Object, cont->emitter, args[1].As<Object>());

  NanSetInternalFieldPointer(NanPersistentToLocal(cont->proxy), 0, cont);

  NanMakeWeak(cont->target, reinterpret_cast<void *>(cont), TargetCallback);

  NanReturnValue(proxy);
}

/**
 * TODO: Make this better.
 */

bool isWeakRef (Handle<Value> val) {
  return val->IsObject() && val.As<Object>()->InternalFieldCount() == 1;
}

/**
 * `isWeakRef()` JS function.
 */

NAN_METHOD(IsWeakRef) {
  NanScope();
  NanReturnValue(Boolean::New(isWeakRef(args[0])));
}

#define WEAKREF_FIRST_ARG                                    \
  if (!isWeakRef(args[0])) {                                 \
    return NanThrowTypeError("Weakref instance expected");   \
  }                                                          \
  Local<Object> proxy = args[0].As<Object>();

/**
 * `get(weakref)` JS function.
 */

NAN_METHOD(Get) {
  NanScope();
  WEAKREF_FIRST_ARG
  if (IsDead(proxy)) NanReturnUndefined();
  NanReturnValue(Unwrap(proxy));
}

/**
 * `isNearDeath(weakref)` JS function.
 */

NAN_METHOD(IsNearDeath) {
  NanScope();
  WEAKREF_FIRST_ARG

  proxy_container *cont = reinterpret_cast<proxy_container*>(
    NanGetInternalFieldPointer(proxy, 0)
  );
  assert(cont != NULL);

  Handle<Boolean> rtn = Boolean::New(cont->target.IsNearDeath());

  NanReturnValue(rtn);
}

/**
 * `isDead(weakref)` JS function.
 */

NAN_METHOD(IsDead) {
  NanScope();
  WEAKREF_FIRST_ARG
  NanReturnValue(Boolean::New(IsDead(proxy)));
}

/**
 * `_getEmitter(weakref)` JS function.
 */

NAN_METHOD(GetEmitter) {
  NanScope();
  WEAKREF_FIRST_ARG
  NanReturnValue(GetEmitter(proxy));
}

/**
 * Sets the global weak callback function.
 */

NAN_METHOD(SetCallback) {
  NanScope();
  Local<Function> callbackHandle = args[0].As<Function>();
  globalCallback = new NanCallback(callbackHandle);
  NanReturnUndefined();
}

/**
 * Init function.
 */

void Initialize(Handle<Object> exports) {
  NanScope();

  Handle<ObjectTemplate> p = ObjectTemplate::New();
  NanAssignPersistent(ObjectTemplate, proxyClass, p);
  p->SetNamedPropertyHandler(WeakNamedPropertyGetter,
                             WeakNamedPropertySetter,
                             WeakNamedPropertyQuery,
                             WeakNamedPropertyDeleter,
                             WeakPropertyEnumerator);
  p->SetIndexedPropertyHandler(WeakIndexedPropertyGetter,
                               WeakIndexedPropertySetter,
                               WeakIndexedPropertyQuery,
                               WeakIndexedPropertyDeleter,
                               WeakPropertyEnumerator);
  p->SetInternalFieldCount(1);

  NODE_SET_METHOD(exports, "get", Get);
  NODE_SET_METHOD(exports, "isWeakRef", IsWeakRef);
  NODE_SET_METHOD(exports, "isNearDeath", IsNearDeath);
  NODE_SET_METHOD(exports, "isDead", IsDead);
  NODE_SET_METHOD(exports, "_create", Create);
  NODE_SET_METHOD(exports, "_getEmitter", GetEmitter);
  NODE_SET_METHOD(exports, "_setCallback", SetCallback);
}

} // anonymous namespace

NODE_MODULE(weakref, Initialize);
