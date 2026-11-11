# R0 step 6 follow-up: the content move left stale package files at the old root for assets whose
# files were still open. Each one must have a twin at the new root and no referencer outside the
# old root; only then is the old root deleted through the editor API.
import unreal

OLD_ROOT = '/Game/' + 'Kni' + 'ght'
NEW_ROOT = '/Game/Golem'
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
eal = unreal.EditorAssetLibrary


def log(message):
    unreal.log('R0RemoveStale: ' + message)


options = unreal.AssetRegistryDependencyOptions()
blocked = []
left = registry.get_assets_by_path(OLD_ROOT, recursive=True, include_only_on_disk_assets=True)
for data in left:
    name = str(data.package_name)
    twin = NEW_ROOT + name[len(OLD_ROOT):]
    referencers = [str(r) for r in (registry.get_referencers(name, options) or []) if not str(r).startswith(OLD_ROOT + '/')]
    has_twin = eal.does_asset_exist(twin)
    log('%s twin=%s outside_referencers=%s' % (name, has_twin, referencers[:4]))
    if not has_twin or referencers:
        blocked.append(name)
log('stale=%d blocked=%d' % (len(left), len(blocked)))
if blocked:
    raise RuntimeError('cannot remove: %s' % blocked[:5])
if eal.does_directory_exist(OLD_ROOT):
    log('delete old root: %s' % eal.delete_directory(OLD_ROOT))
remaining = registry.get_assets_by_path(OLD_ROOT, recursive=True, include_only_on_disk_assets=True)
log('remaining=%d' % len(remaining))
if remaining:
    raise RuntimeError('old root still has assets')
log('DONE')
