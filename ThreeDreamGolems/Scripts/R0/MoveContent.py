# R0 step 6: run inside the editor (-run=pythonscript -script=<this file>).
# Moves the old content root to /Game/Golem with the editor rename API, renames assets that carry the old
# word, fixes up redirectors and saves. It never touches files on disk directly.
import unreal

OLD_ROOT = '/Game/' + 'Kni' + 'ght'
NEW_ROOT = '/Game/Golem'
OLD_WORD = 'Kni' + 'ght'

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(message):
    unreal.log('R0MoveContent: ' + message)


def assets_under(root):
    return registry.get_assets_by_path(root, recursive=True, include_only_on_disk_assets=True)


before = assets_under(OLD_ROOT)
log('assets before: %d' % len(before))
renames = []
for data in before:
    asset = data.get_asset()
    if asset is None:
        raise RuntimeError('cannot load %s' % data.package_name)
    old_path = str(data.package_path)
    new_path = NEW_ROOT + old_path[len(OLD_ROOT):]
    new_name = str(data.asset_name).replace(OLD_WORD, 'Golem')
    renames.append(unreal.AssetRenameData(asset, new_path, new_name))
if renames and not tools.rename_assets(renames):
    raise RuntimeError('rename_assets failed')

# Fix up and remove the redirectors left at the old location.
redirectors = []
for data in registry.get_assets_by_path(OLD_ROOT, recursive=True):
    if str(data.asset_class_path.asset_name) == 'ObjectRedirector':
        redirector = data.get_asset()
        if redirector is not None:
            redirectors.append(redirector)
log('redirectors: %d' % len(redirectors))
if redirectors:
    tools.fixup_referencers(redirectors, False, unreal.RedirectFixupMode.DELETE_FIXED_UP_REDIRECTORS)
eal.save_directory(NEW_ROOT, only_if_is_dirty=False, recursive=True)

left = [str(d.package_name) for d in registry.get_assets_by_path(OLD_ROOT, recursive=True)]
after = assets_under(NEW_ROOT)
stale = [str(d.asset_name) for d in after if OLD_WORD in str(d.asset_name)]
log('assets after: %d, left at old root: %d, old-word names: %d' % (len(after), len(left), len(stale)))
for name in left[:50]:
    log('LEFT ' + name)
if not left and eal.does_directory_exist(OLD_ROOT):
    eal.delete_directory(OLD_ROOT)
if left or stale or len(after) != len(before):
    raise RuntimeError('content move incomplete')
log('DONE')
