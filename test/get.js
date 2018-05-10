var assert = require('assert');
var weak = require('../');

describe('weak.get()', function () {

  afterEach(gc);

  it('should return the target if it is still accessible', function () {
    var obj = {};
    var r = weak(obj);
    assert.equal(obj, weak.get(r));
  });

  it('should return undefined after the target is gc\'d', function () {
    var r = weak({});
    gc();
    assert.equal(undefined, weak.get(r));
  });

  it('should not crash inside garbage collection callback', function () {
    var result;
    var r = weak({}, function () {
      result = typeof(weak.get(r));
    });
    gc();
  });

});
