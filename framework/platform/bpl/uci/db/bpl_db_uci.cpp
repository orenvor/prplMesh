#include "bpl_db_uci.h"

#include <mapf/common/logger.h>
#include <mapf/common/utils.h>

extern "C" {
#include <uci.h>
}

namespace beerocks {
namespace bpl {
namespace db {

bool uci_entry_exists(const std::string &config_file, const std::string &entry_type,
                      const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        uci_free_context(ctx);
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_package *pkg = nullptr;
    if (!(pkg = uci_lookup_package(ctx, config_file.c_str()))) {
        // Try and load the package, incase it wasn't loaded before
        uci_load(ctx, config_file.c_str(), &pkg);
        if (!pkg) {
            uci_free_context(ctx);
            LOG(ERROR) << "uci lookup package failed!";
            return false;
        }
    }

    // Loop through the sections with a matching section name (entry_name)
    struct uci_element *elm = nullptr;
    while (uci_lookup_next(ctx, &elm, &pkg->sections, entry_name.c_str()) == UCI_OK) {
        struct uci_section *sec = uci_to_section(elm);
        // if the entry_type is empty all matching sections are a valid match
        if (entry_type.empty() || entry_type.compare(sec->type)) {
            uci_free_context(ctx);
            return true;
        }
    }

    uci_free_context(ctx);
    LOG(TRACE) << "No " << entry_name << " entry found" << entry_type.empty()
        ? "!"
        : std::string(" with type: ") + entry_type;

    return false;
}

bool uci_add_entry(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    if (uci_entry_exists(config_file, entry_type, entry_name)) {
        LOG(TRACE) << "uci entry already exists!";
        return true;
    }

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        uci_free_context(ctx);
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    ptr.package = config_file.c_str();
    ptr.section = entry_name.c_str();
    ptr.option  = nullptr;
    ptr.value   = entry_type.c_str();

    if (uci_set(ctx, &ptr) != UCI_OK) {
        uci_free_context(ctx);
        LOG(ERROR) << "Failed to create entry!";
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
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    if (!uci_entry_exists(config_file, entry_type, entry_name)) {
        LOG(TRACE) << "uci entry does not exists!";
        return true;
    }

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    for (auto &param : params) {
        struct uci_ptr ptr;
        ptr.package = config_file.c_str();
        ptr.section = entry_name.c_str();
        ptr.option  = param.first.c_str();  //option
        ptr.value   = param.second.c_str(); //value

        if (uci_set(ctx, &ptr) != UCI_OK) {
            uci_free_context(ctx);
            LOG(ERROR) << "Failed to set option " << param.first << "!";
            return false;
        }
    }

    uci_free_context(ctx);
    return true;
}

bool uci_get_entry(const std::string &config_file, const std::string &entry_type,
                   const std::string &entry_name,
                   std::unordered_map<std::string, std::string> &params)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto str        = config_file + "." + entry_name;
    auto lookup_str = const_cast<char *>(str.c_str());
    if (uci_lookup_ptr(ctx, &ptr, lookup_str, true) != UCI_OK || !ptr.s) {
        LOG(ERROR) << "UCI failed to lookup ptr for path: " << lookup_str;
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
    LOG(TRACE) << "entry: " << config_file << ": " << entry_name;

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path = const_cast<char *>(std::string(config_file + "." + entry_name).c_str());
    if (uci_lookup_ptr(ctx, &ptr, path, true) != UCI_OK || !ptr.s) {
        uci_free_context(ctx);
        LOG(ERROR) << "uci lookup package failed!";
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
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")"
               << ", " << param_name;

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path =
        const_cast<char *>(std::string(config_file + "." + entry_name + "." + param_name).c_str());

    if (uci_lookup_ptr(ctx, &ptr, path, false) != UCI_OK || !ptr.o) {
        LOG(ERROR) << "uci lookup package failed!";
        return false;
    }

    if (ptr.o->type != UCI_TYPE_STRING) {
        uci_free_context(ctx);
        LOG(ERROR) << "list values are not supported!";
        return false;
    }

    value = ptr.o->v.string;
    return true;
}

bool uci_delete_entry(const std::string &config_file, const std::string &entry_type,
                      const std::string &entry_name)
{
    //entry: config_file: entry_type(entry_name)
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type << "(" << entry_name << ")";

    struct uci_context *ctx = nullptr;
    if (!(ctx = uci_alloc_context())) {
        uci_free_context(ctx);
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto path = const_cast<char *>(std::string(config_file + "." + entry_name).c_str());
    if (uci_lookup_ptr(ctx, &ptr, path, false) != UCI_OK || !ptr.s) {
        LOG(ERROR) << "uci lookup failed!";
        return false;
    }

    uci_delete(ctx, &ptr);
    return true;
}

bool uci_get_all_entries(const std::string &config_file, const std::string &entry_type,
                         std::vector<std::string> &entries)
{
    //entry: config_file: entry_type
    LOG(TRACE) << "entry: " << config_file << ": " << entry_type;

    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOG(ERROR) << "uci context allocate failed!";
        return false;
    }

    struct uci_ptr ptr;
    auto lookup_str = const_cast<char *>(config_file.c_str());
    if ((uci_lookup_ptr(ctx, &ptr, lookup_str, true) != UCI_OK)) {
        LOG(ERROR) << "UCI lookup failed for path: " << lookup_str;
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
