#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstring>
#include <elf.h>

namespace sys::elf {

/**
 * @brief Segment Permission Flags.
 */
struct SegmentFlags {
    bool read{false};
    bool write{false};
    bool execute{false};

    [[nodiscard]] std::string to_string() const {
        std::string s;
        s += (read ? 'R' : '-');
        s += (write ? 'W' : '-');
        s += (execute ? 'X' : '-');
        return s;
    }
};

struct ProgramSegment {
    uint32_t type;
    std::string type_name;
    SegmentFlags flags;
    uint64_t file_offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t align;
};

struct SectionInfo {
    std::string name;
    uint32_t type;
    std::string type_name;
    uint64_t flags;
    std::string flags_str;
    uint64_t vaddr;
    uint64_t file_offset;
    uint64_t size;
    uint64_t align;
};

/**
 * @brief Self-Contained 64-Bit ELF Executable Inspector & Parser.
 */
class Elf64Inspector {
public:
    explicit Elf64Inspector(std::vector<uint8_t> buffer)
        : buffer_(std::move(buffer)) {
        parse();
    }

    [[nodiscard]] static Elf64Inspector from_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open ELF file: " + path);
        }

        const auto size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buf(size);
        if (!file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size))) {
            throw std::runtime_error("Failed to read ELF file data: " + path);
        }

        return Elf64Inspector(std::move(buf));
    }

    [[nodiscard]] bool is_valid() const noexcept { return is_valid_; }
    [[nodiscard]] uint64_t entry_point() const noexcept { return header_.e_entry; }
    [[nodiscard]] uint16_t type() const noexcept { return header_.e_type; }
    [[nodiscard]] uint16_t machine() const noexcept { return header_.e_machine; }
    [[nodiscard]] const std::vector<ProgramSegment>& segments() const noexcept { return segments_; }
    [[nodiscard]] const std::vector<SectionInfo>& sections() const noexcept { return sections_; }

    void print_summary(std::ostream& os = std::cout) const {
        if (!is_valid_) {
            os << "[ERROR] Invalid ELF64 binary image.\n";
            return;
        }

        os << "========================================================================\n"
           << " ELF64 Binary Inspection Report\n"
           << "========================================================================\n"
           << " Type:             " << get_elf_type_name(header_.e_type) << " (0x" << std::hex << header_.e_type << ")\n"
           << " Machine:          " << get_machine_name(header_.e_machine) << " (0x" << header_.e_machine << ")\n"
           << " Entry Point:      0x" << std::setw(16) << std::setfill('0') << header_.e_entry << std::dec << "\n"
           << " Program Headers:  " << header_.e_phnum << " entries (Offset: 0x" << std::hex << header_.e_phoff << ")\n"
           << " Section Headers:  " << header_.e_shnum << " entries (Offset: 0x" << header_.e_shoff << ")\n"
           << std::dec << "\n";

        // Program Headers Table
        os << "------------------------------------------------------------------------\n"
           << " Program Headers (Runtime Memory Segments)\n"
           << "------------------------------------------------------------------------\n"
           << " Type           Perm  Offset             VirtAddr           MemSize    Align\n"
           << "------------------------------------------------------------------------\n";

        for (const auto& seg : segments_) {
            os << std::left << std::setw(16) << std::setfill(' ') << seg.type_name << " "
               << seg.flags.to_string() << "  "
               << "0x" << std::right << std::hex << std::setw(16) << std::setfill('0') << seg.file_offset << " "
               << "0x" << std::setw(16) << seg.vaddr << " "
               << std::dec << std::setw(9) << std::setfill(' ') << seg.mem_size << " "
               << "0x" << std::hex << seg.align << std::dec << "\n";
        }

        // Section Headers Table
        os << "\n------------------------------------------------------------------------\n"
           << " Section Headers (Linking & Symbol Anatomy)\n"
           << "------------------------------------------------------------------------\n"
           << " Name                 Type             VirtAddr           Size       Flags\n"
           << "------------------------------------------------------------------------\n";

        for (const auto& sec : sections_) {
            os << std::left << std::setw(21) << std::setfill(' ') << (sec.name.empty() ? "[NULL]" : sec.name) << " "
               << std::setw(16) << sec.type_name << " "
               << "0x" << std::right << std::hex << std::setw(16) << std::setfill('0') << sec.vaddr << " "
               << std::dec << std::setw(10) << std::setfill(' ') << sec.size << " "
               << sec.flags_str << "\n";
        }

        os << "========================================================================\n\n";
    }

private:
    void parse() {
        if (buffer_.size() < sizeof(Elf64_Ehdr)) {
            is_valid_ = false;
            return;
        }

        std::memcpy(&header_, buffer_.data(), sizeof(Elf64_Ehdr));

        // Verify ELF Magic: 0x7F, 'E', 'L', 'F'
        if (header_.e_ident[EI_MAG0] != ELFMAG0 ||
            header_.e_ident[EI_MAG1] != ELFMAG1 ||
            header_.e_ident[EI_MAG2] != ELFMAG2 ||
            header_.e_ident[EI_MAG3] != ELFMAG3) {
            is_valid_ = false;
            return;
        }

        // Verify 64-bit architecture
        if (header_.e_ident[EI_CLASS] != ELFCLASS64) {
            is_valid_ = false;
            return;
        }

        is_valid_ = true;
        parse_program_headers();
        parse_section_headers();
    }

    void parse_program_headers() {
        if (header_.e_phoff == 0 || header_.e_phnum == 0) return;

        const size_t ph_table_end = header_.e_phoff + (static_cast<size_t>(header_.e_phnum) * sizeof(Elf64_Phdr));
        if (ph_table_end > buffer_.size()) return;

        for (size_t i = 0; i < header_.e_phnum; ++i) {
            Elf64_Phdr phdr;
            const size_t offset = header_.e_phoff + (i * sizeof(Elf64_Phdr));
            std::memcpy(&phdr, buffer_.data() + offset, sizeof(Elf64_Phdr));

            ProgramSegment seg;
            seg.type = phdr.p_type;
            seg.type_name = get_segment_type_name(phdr.p_type);
            seg.flags.read = (phdr.p_flags & PF_R) != 0;
            seg.flags.write = (phdr.p_flags & PF_W) != 0;
            seg.flags.execute = (phdr.p_flags & PF_X) != 0;
            seg.file_offset = phdr.p_offset;
            seg.vaddr = phdr.p_vaddr;
            seg.paddr = phdr.p_paddr;
            seg.file_size = phdr.p_filesz;
            seg.mem_size = phdr.p_memsz;
            seg.align = phdr.p_align;

            segments_.push_back(seg);
        }
    }

    void parse_section_headers() {
        if (header_.e_shoff == 0 || header_.e_shnum == 0) return;

        const size_t sh_table_end = header_.e_shoff + (static_cast<size_t>(header_.e_shnum) * sizeof(Elf64_Shdr));
        if (sh_table_end > buffer_.size()) return;

        // Extract section header string table
        const char* strtab = nullptr;
        if (header_.e_shstrndx < header_.e_shnum) {
            Elf64_Shdr strtab_hdr;
            const size_t strtab_offset = header_.e_shoff + (header_.e_shstrndx * sizeof(Elf64_Shdr));
            std::memcpy(&strtab_hdr, buffer_.data() + strtab_offset, sizeof(Elf64_Shdr));
            if (strtab_hdr.sh_offset + strtab_hdr.sh_size <= buffer_.size()) {
                strtab = reinterpret_cast<const char*>(buffer_.data() + strtab_hdr.sh_offset);
            }
        }

        for (size_t i = 0; i < header_.e_shnum; ++i) {
            Elf64_Shdr shdr;
            const size_t offset = header_.e_shoff + (i * sizeof(Elf64_Shdr));
            std::memcpy(&shdr, buffer_.data() + offset, sizeof(Elf64_Shdr));

            SectionInfo sec;
            if (strtab && shdr.sh_name != 0) {
                sec.name = strtab + shdr.sh_name;
            } else {
                sec.name = "";
            }

            sec.type = shdr.sh_type;
            sec.type_name = get_section_type_name(shdr.sh_type);
            sec.flags = shdr.sh_flags;
            sec.flags_str = get_section_flags_str(shdr.sh_flags);
            sec.vaddr = shdr.sh_addr;
            sec.file_offset = shdr.sh_offset;
            sec.size = shdr.sh_size;
            sec.align = shdr.sh_addralign;

            sections_.push_back(sec);
        }
    }

    static std::string get_elf_type_name(uint16_t type) {
        switch (type) {
            case ET_NONE: return "NONE (Unknown)";
            case ET_REL:  return "REL (Relocatable object)";
            case ET_EXEC: return "EXEC (Executable)";
            case ET_DYN:  return "DYN (Position-Independent Executable / Shared Object)";
            case ET_CORE: return "CORE (Core dump)";
            default:      return "OTHER";
        }
    }

    static std::string get_machine_name(uint16_t machine) {
        switch (machine) {
            case EM_X86_64: return "Advanced Micro Devices X86-64";
            case EM_386:    return "Intel 80386";
            case EM_AARCH64: return "ARM AArch64";
            default:        return "Unknown Machine";
        }
    }

    static std::string get_segment_type_name(uint32_t type) {
        switch (type) {
            case PT_NULL:         return "PT_NULL";
            case PT_LOAD:         return "PT_LOAD";
            case PT_DYNAMIC:      return "PT_DYNAMIC";
            case PT_INTERP:       return "PT_INTERP";
            case PT_NOTE:         return "PT_NOTE";
            case PT_SHLIB:        return "PT_SHLIB";
            case PT_PHDR:         return "PT_PHDR";
            case PT_TLS:          return "PT_TLS";
            case PT_GNU_EH_FRAME: return "PT_GNU_EH_FRAME";
            case PT_GNU_STACK:    return "PT_GNU_STACK";
            case PT_GNU_RELRO:    return "PT_GNU_RELRO";
            case PT_GNU_PROPERTY: return "PT_GNU_PROPERTY";
            default:              return "PT_OTHER (0x" + std::to_string(type) + ")";
        }
    }

    static std::string get_section_type_name(uint32_t type) {
        switch (type) {
            case SHT_NULL:          return "SHT_NULL";
            case SHT_PROGBITS:      return "SHT_PROGBITS";
            case SHT_SYMTAB:        return "SHT_SYMTAB";
            case SHT_STRTAB:        return "SHT_STRTAB";
            case SHT_RELA:          return "SHT_RELA";
            case SHT_HASH:          return "SHT_HASH";
            case SHT_DYNAMIC:       return "SHT_DYNAMIC";
            case SHT_NOTE:          return "SHT_NOTE";
            case SHT_NOBITS:        return "SHT_NOBITS";
            case SHT_REL:           return "SHT_REL";
            case SHT_DYNSYM:        return "SHT_DYNSYM";
            case SHT_INIT_ARRAY:    return "SHT_INIT_ARRAY";
            case SHT_FINI_ARRAY:    return "SHT_FINI_ARRAY";
            case SHT_GNU_HASH:      return "SHT_GNU_HASH";
            case SHT_GNU_verdef:    return "SHT_GNU_verdef";
            case SHT_GNU_verneed:   return "SHT_GNU_verneed";
            case SHT_GNU_versym:    return "SHT_GNU_versym";
            default:                return "SHT_OTHER";
        }
    }

    static std::string get_section_flags_str(uint64_t flags) {
        std::string s = "[";
        if (flags & SHF_WRITE)            s += 'W';
        if (flags & SHF_ALLOC)            s += 'A';
        if (flags & SHF_EXECINSTR)        s += 'X';
        if (flags & SHF_MERGE)            s += 'M';
        if (flags & SHF_STRINGS)          s += 'S';
        if (flags & SHF_INFO_LINK)        s += 'I';
        if (flags & SHF_LINK_ORDER)       s += 'L';
        if (flags & SHF_OS_NONCONFORMING) s += 'O';
        if (flags & SHF_GROUP)            s += 'G';
        if (flags & SHF_TLS)              s += 'T';
        s += ']';
        return s;
    }

    std::vector<uint8_t> buffer_;
    Elf64_Ehdr header_{};
    std::vector<ProgramSegment> segments_;
    std::vector<SectionInfo> sections_;
    bool is_valid_{false};
};

} // namespace sys::elf
