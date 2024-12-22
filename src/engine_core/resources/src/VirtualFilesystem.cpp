/*! \file VirtualFilesystem.cpp
    \author Alan Ramirez
    \date 2024-12-07
    \brief A Virtual filesystem implementation
*/

#include "VirtualFilesystem.hpp"

#include <ranges>

Hush::VirtualFilesystem::VirtualFilesystem() = default;

Hush::VirtualFilesystem::~VirtualFilesystem() = default;

Hush::VirtualFilesystem::VirtualFilesystem(VirtualFilesystem &&rhs) noexcept
    : m_mountedFileSystems(std::move(rhs.m_mountedFileSystems))
{
}

Hush::VirtualFilesystem &Hush::VirtualFilesystem::operator=(VirtualFilesystem &&rhs) noexcept
{
    if (this != &rhs)
    {
        m_mountedFileSystems = std::move(rhs.m_mountedFileSystems);
    }

    return *this;
}
void Hush::VirtualFilesystem::Unmount(std::string_view virtualPath)
{
    m_mountedFileSystems.erase(
        std::remove_if(m_mountedFileSystems.begin(), m_mountedFileSystems.end(),
                       [&](const MountPoint &mountPoint) { return mountPoint.path == virtualPath; }));
}
std::vector<std::string_view> Hush::VirtualFilesystem::ListPath(std::string_view virtualPath, EListOptions options)
{
    (void)virtualPath;
    (void)options;
    return {};
}

Hush::Result<std::span<std::byte>, Hush::VirtualFilesystem::EError> Hush::VirtualFilesystem::ReadFile(
    std::string_view virtualPath)
{
    auto resolved = ResolveFileSystem(virtualPath);
    if (!resolved)
    {
        return EError::FileDoesntExist;
    }
    auto &filesystem = resolved->filesystem;
    auto &path = resolved->path;

    auto data = filesystem->LoadData(path);

    if (!data)
    {
        return EError::FileDoesntExist;
    }

    return data.assume_value();
}

void Hush::VirtualFilesystem::MountFileSystemInternal(std::string_view path,
                                                      std::unique_ptr<IFileSystem> resourceLoader)
{
    m_mountedFileSystems.emplace_back(std::string(path), std::move(resourceLoader));
}

std::optional<Hush::VirtualFilesystem::ResolvedPath> Hush::VirtualFilesystem::ResolveFileSystem(std::string_view path)
{
    // We need to iterate on all filesystems in backward order.
    for (auto start = this->m_mountedFileSystems.begin(); start != this->m_mountedFileSystems.end(); ++start)
    {
        auto &mountPoint = start->path;
        auto &filesystem = start->filesystem;

        if (path.starts_with(mountPoint))
        {
            auto relativePath = path.substr(mountPoint.size());

            return ResolvedPath{
                .filesystem = filesystem.get(),
                .path = relativePath,
            };
        }
    }

    return {};
}