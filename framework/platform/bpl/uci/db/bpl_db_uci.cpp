#include "bpl_db_uci.h"

#include <mapf/common/logger.h>
#include <mapf/common/utils.h>

extern "C" {
#include <uci.h>
}

namespace beerocks {
namespace bpl {
namespace db {

std::string uci_get_error(struct uci_context *ctx)
{
    char *err_buf;
    uci_get_errorstr(ctx, &err_buf, "");
    return std::string(err_buf);
}

std::string compose_path(const std::string &config_file) { return config_file; }
std::string compose_path(const std::string &config_file, const std::string &entry_name)
{
    return compose_path(config_file) + "." + entry_name;
}
std::string compose_path(const std::string &config_file, const std::string &entry_name,
                         const std::string &param_name)
{
    return compose_path(config_file, entry_name) + "." + entry_name;
}
std::string compose_path(const std::string &config_file, const std::string &entry_name,
                         const std::string &param_name, const std::string param_value)
{
    return compose_path(config_file, entry_name, param_name) + "=" + entry_name;
}

bool uci_entry_exists(const std::string &config_file, const std::string &entry_type,
                      const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "uci_entry_exists() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        uci_free_context(ctx);
        return false;
    }

    struct uci_package *pkg = nullptr;
    if (!(pkg = uci_lookup_package(ctx, config_file.c_str()))) {
        // Try and load the package, incase it wasn't loaded before
        uci_load(ctx, config_file.c_str(), &pkg);
        if (!pkg) {
            LOG(ERROR) << "uci lookup package failed!" << std::endl << uci_get_error(ctx);
            uci_free_context(ctx);
            return false;
        }
    }

    bool found = false;
    // Loop through the sections with a matching section name (entry_name)
    struct uci_element *elm = nullptr;
    uci_foreach_element(&pkg->sections, elm)
    {
        found                   = true;
        struct uci_section *sec = uci_to_section(elm);
        // if the entry_type is empty all matching sections are a valid match
        if (entry_type.empty() || entry_type.compare(sec->type) == 0) {
            uci_free_context(ctx);
            return true;
        }
    }

    uci_free_context(ctx);
    LOG(TRACE) << "No " << entry_name << " entry found"
               << (entry_type.empty() || !found ? "!" : std::string(" with type: ") + entry_type);

    return false;
}

bool uci_add_entry(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "uci_add_entry() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    if (uci_entry_exists(config_file, entry_type, entry_name)) {
        LOG(TRACE) << "uci entry already exists!";
        return true;
    }

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    ptr.package = config_file.c_str();
    ptr.section = entry_name.c_str();
    ptr.option  = nullptr;
    ptr.value   = entry_type.c_str();

    if (uci_set(ctx, &ptr) != UCI_OK) {
        LOG(ERROR) << "Failed to create entry!" << std::endl << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    uci_free_context(ctx);
    return true;
}

bool uci_set_entry(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name,
                   const std::unordered_map<std::string, std::string> &params)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "uci_set_entry() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr pkg_ptr;
    auto pkg_path       = compose_path(config_file);
    auto pkg_lookup_str = const_cast<char *>(pkg_path.c_str());
    // Lookup package to save delta after changes are made.
    if (uci_lookup_ptr(ctx, &pkg_ptr, pkg_lookup_str, true) != UCI_OK) {
        LOG(ERROR) << "UCI failed to lookup ptr for path: " << pkg_lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    for (auto &param : params) {
        struct uci_ptr opt_ptr;
        auto opt_path       = compose_path(config_file, entry_name, param.first);
        auto opt_lookup_str = const_cast<char *>(opt_path.c_str());

        // Get pointer to value
        if (uci_lookup_ptr(ctx, &opt_ptr, opt_lookup_str, true) != UCI_OK) {
            LOG(ERROR) << "UCI failed to lookup ptr for path: " << opt_lookup_str << std::endl
                       << uci_get_error(ctx);
            uci_free_context(ctx);
            return false;
        }

        LOG(TRACE) << "Found " << opt_path;
        //Set the actual value
        opt_ptr.value = strdup(param.second.c_str());
        LOG(TRACE) << "Setting " << opt_path << "=" << param.second;

        // If option does not exist, 'uci_set' creates it.
        if (uci_set(ctx, &opt_ptr) != UCI_OK) {
            LOG(ERROR) << "Failed to set option " << param.first << "!";
            uci_free_context(ctx);
            return false;
        } else {
            // // Create delta from changes, this does not change the file.
            // if (uci_save(ctx, opt_ptr.p) != UCI_OK) {
            //     LOG(ERROR) << "Failed to save changes!" << std::endl << uci_get_error(ctx);
            //     uci_free_context(ctx);
            //     return false;
            // }
        }
    }
    LOG(TRACE) << "Finished setting parameters. about to commit changes";

    // Commit changes to file
    if (uci_commit(ctx, &pkg_ptr.p, false) != UCI_OK) {
        LOG(ERROR) << "Failed to commit changes!" << std::endl << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }
    uci_free_context(ctx);
    return true;
}

bool uci_get_entry(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name,
                   std::unordered_map<std::string, std::string> &params)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "uci_get_entry() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path       = compose_path(config_file, entry_name);
    auto lookup_str = const_cast<char *>(path.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, true) != UCI_OK || !ptr.s) {
        LOG(ERROR) << "UCI failed to lookup ptr for path: " << lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    // Loop through the option within the found section
    struct uci_element *elm = nullptr;
    uci_foreach_element(&ptr.s->options, elm)
    {
        struct uci_option *opt = uci_to_option(elm);
        // Only UCI_TYPE_STRING is supported
        if (opt->type == UCI_TYPE_STRING) {
            params[std::string(opt->e.name)] = opt->v.string;
        }
    }

    uci_free_context(ctx);
    return true;
}

bool uci_get_entry_type(const std::string &config_file, const std::string &entry_name,
                        std::string &entry_type)
{
    //entry: config_file: entry_name
    LOG(TRACE) << "uci_get_entry_type() "
               << "entry: " << config_file << ": " << entry_name;

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path       = compose_path(config_file, entry_name);
    auto lookup_str = const_cast<char *>(path.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, true) != UCI_OK || !ptr.s) {
        LOG(ERROR) << "UCI lookup failed for path: " << lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    entry_type = ptr.s->type;
    uci_free_context(ctx);
    return true;
}

bool uci_get_param(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name, const std::string &param_name, std::string &value)
{
    //entry: config_file: entry_type(entry_name), param
    LOG(TRACE) << "uci_get_param() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")"
               << ", " << param_name;

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path       = compose_path(config_file, entry_name);
    auto lookup_str = const_cast<char *>(path.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, false) != UCI_OK || !ptr.o) {
        LOG(ERROR) << "UCI lookup failed for path: " << lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    if (ptr.o->type != UCI_TYPE_STRING) {
        LOG(ERROR) << "list values are not supported!" << std::endl << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    value = ptr.o->v.string;
    uci_free_context(ctx);
    return true;
}

bool uci_delete_entry(const std::string &config_file, const std::string &entry_type,
                      const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "uci_delete_entry() "
               << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path       = compose_path(config_file, entry_name);
    auto lookup_str = const_cast<char *>(path.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, false) != UCI_OK || !ptr.s) {
        LOG(ERROR) << "UCI lookup failed for path: " << lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    uci_delete(ctx, &ptr);
    uci_free_context(ctx);
    return true;
}

bool uci_get_all_entries(const std::string &config_file, const std::string &entry_type,
                         std::vector<std::string> &entries)
{
    //entry: config_file: entry_type
    LOG(TRACE) << "uci_get_all_entries() "
               << "entry: " << config_file << ": " << entry_type;

    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path       = compose_path(config_file);
    auto lookup_str = const_cast<char *>(path.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, false) != UCI_OK) {
        LOG(ERROR) << "UCI lookup failed for path: " << lookup_str << std::endl
                   << uci_get_error(ctx);
        uci_free_context(ctx);
        return false;
    }

    // Loop through the sections with a matching section name (entry_name)
    struct uci_element *elm = nullptr;
    uci_foreach_element(&ptr.p->sections, elm)
    {
        struct uci_section *sec = uci_to_section(elm);
        if (entry_type.empty() || entry_type.compare(sec->type) == 0) {
            entries.emplace_back(sec->e.name);
        }
    }
    LOG(DEBUG) << "Found " << entries.size() << " entries.";

    uci_free_context(ctx);
    return true;
}

} // namespace db
} // namespace bpl
} // namespace beerocks
