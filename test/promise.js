var assert = require('assert');
var weak = require('../')

describe('promise', function () {
  if (!Promise) {
    // Version of JS without `Promise`; nothing to do.
    return;
  }

  it('should not mistake a promise for a weak reference', function () {
    var p = new Promise(function () {});
    assert(!weak.isWeakRef(p));
  });
})
