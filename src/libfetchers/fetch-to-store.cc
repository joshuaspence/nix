#include "nix/fetchers/fetch-to-store.hh"
#include "nix/fetchers/fetchers.hh"
#include "nix/fetchers/fetch-settings.hh"
#include "nix/util/posix-source-accessor.hh"

namespace nix {

fetchers::Cache::Key makeFetchToStoreCacheKey(
    const std::string & name, const std::string & fingerprint, ContentAddressMethod method, const std::string & path)
{
    return fetchers::Cache::Key{
        "fetchToStore",
        {{"name", name}, {"fingerprint", fingerprint}, {"method", std::string{method.render()}}, {"path", path}}};
}

StorePath fetchToStore(
    const fetchers::Settings & settings,
    Store & store,
    const SourcePath & path,
    FetchMode mode,
    std::string_view name,
    ContentAddressMethod method,
    PathFilter * filter,
    RepairFlag repair)
{
    // Optimization: if the accessor is a PosixSourceAccessor pointing to a store path,
    // we can return that store path directly without copying.
    // Only apply this optimization when we're in Copy mode and there's no filter.
    if (!filter && mode == FetchMode::Copy) {
        if (auto * posixAccessor = dynamic_cast<PosixSourceAccessor *>(&*path.accessor)) {
            if (auto physicalPath = posixAccessor->getPhysicalPath(path.path)) {
                auto pathStr = physicalPath->string();
                if (store.isInStore(pathStr)) {
                    try {
                        auto [storePath, subPath] = store.toStorePath(pathStr);
                        // Only return the store path if:
                        // 1. It's the exact path (no sub-path)
                        // 2. The store path is valid
                        // 3. It's not a derivation (to avoid issues with import-from-derivation)
                        if (subPath.empty() && store.isValidPath(storePath) && !storePath.isDerivation()) {
                            debug("optimized store path copy for '%s' - already in store as '%s'",
                                  path, store.printStorePath(storePath));
                            return storePath;
                        }
                    } catch (const Error &) {
                        // Not a valid store path, fall through to normal processing
                    }
                }
            }
        }
    }

    std::optional<fetchers::Cache::Key> cacheKey;

    if (!filter && path.accessor->fingerprint) {
        cacheKey = makeFetchToStoreCacheKey(std::string{name}, *path.accessor->fingerprint, method, path.path.abs());
        if (auto res = settings.getCache()->lookupStorePath(*cacheKey, store)) {
            debug("store path cache hit for '%s'", path);
            return res->storePath;
        }
    } else
        debug("source path '%s' is uncacheable", path);

    Activity act(
        *logger,
        lvlChatty,
        actUnknown,
        fmt(mode == FetchMode::DryRun ? "hashing '%s'" : "copying '%s' to the store", path));

    auto filter2 = filter ? *filter : defaultPathFilter;

    auto storePath = mode == FetchMode::DryRun
                         ? store.computeStorePath(name, path, method, HashAlgorithm::SHA256, {}, filter2).first
                         : store.addToStore(name, path, method, HashAlgorithm::SHA256, {}, filter2, repair);

    debug(mode == FetchMode::DryRun ? "hashed '%s'" : "copied '%s' to '%s'", path, store.printStorePath(storePath));

    if (cacheKey && mode == FetchMode::Copy)
        settings.getCache()->upsert(*cacheKey, store, {}, storePath);

    return storePath;
}

} // namespace nix
