project('numbfs-utils',
        'c',
        version: '0.1',
        default_options: ['warning_level=3'])

#
# Feature detection, such as print debug info, etc.
#
private_cfg = configuration_data()

if get_option('numbfs_debug')
        private_cfg.set('HAVE_NUMBFS_DEBUG', '1')
endif

#
# write private flags to numbfs_config.h
#
configure_file(output: 'numbfs_config.h', configuration : private_cfg)

executable('mkfs.numbfs', ['mkfs.c', 'lib.c'], install: true)
executable('fsck.numbfs', ['fsck.c', 'lib.c'], install: true)

numbfs_test = executable('numbfs_unit_test', ['test.c', 'lib.c'])
test('numbfs_test', numbfs_test)
