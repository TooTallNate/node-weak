var assert = require('assert');
var weak = require('../');

describe('weak.isWeakRef()', function () {

  afterEach(gc);

  it('should be true for a `WeakRef` instance', function () {
    var r = weak({});
    assert(weak.isWeakRef(r));
  });

  it('should be false for a normal object', function () {
    var obj = {};
    assert(!weak.isWeakRef(obj));
  });

});

describe('weak.isDead()', function () {

  afterEach(gc);

  it('should be false before the target is gc\'d', function () {
    var obj = {};
    var r = weak(obj);
    assert(!weak.isDead(r));
  });

  it('should be false inside garbage collection callback', function () {
    var result;
    var r = weak({}, function () {
      result = !weak.isDead(r);
    });
    gc();
    assert(result);
  });

  it('should be true after the target is gc\'d', function () {
    var r = weak({});
    gc();
    assert(weak.isDead(r));
  });

});

describe('weak.isNearDeath()', function () {

  afterEach(gc);

  it('should be false before the target is gc\'d', function () {
    var obj = {};
    var r = weak(obj);
    assert(!weak.isNearDeath(r));
  });

  it('should be true inside garbage collection callback', function () {
    var result;
    var r = weak({}, function () {
      result = weak.isNearDeath(r);
    });
    gc();
    assert(result);
  });

  it('should be false after the target is gc\'d', function () {
    var r = weak({});
    gc();
    assert(!weak.isNearDeath(r));
  });

});
