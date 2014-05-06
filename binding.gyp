{
  'targets': [{
    'target_name': 'weakref',
    'sources': [ 'src/weakref.cc' ],
    # for now, we need to use a custom version of nan...
    #'include_dirs': [
    #  '<!(node -e "require(\'nan\')")'
    #]
  }]
}
