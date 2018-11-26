{
  'targets': [{
    'target_name': 'weakref',
    'sources': [ 'src/weakref.cc' ],
    'include_dirs': [
      '<!(node -e "require(\'nan\')")'
    ],
    'xcode_settings': {
      'MACOSX_DEPLOYMENT_TARGET': '10.9'
    }
  }]
}
