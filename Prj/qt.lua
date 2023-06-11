local p = premake
local api = p.api
local project = p.project

api.register {
    name= "QtSlnConfigs",
    scope = "solution",
    kind = "list:string",
}
api.register {
    name= "QtConfigs",
    scope = "project",
    kind = "list:string",
}
api.register {
    name= "QtModules",
    scope = "project",
    kind = "list:string",
}
api.register {
    name= "AppName",
    scope = "project",
    kind = "string",
}
api.register {
    name= "AppCompany",
    scope = "project",
    kind = "string",
}
api.register {
    name= "AppCopyright",
    scope = "project",
    kind = "string",
}
api.register {
    name= "AppDescription",
    scope = "project",
    kind = "string",
}
api.register {
    name= "AppVersion",
    scope = "project",
    kind = "string",
}
api.register {
    name= "AppIcon",
    scope = "project",
    kind = "file",
}
api.register {
    name= "QtResources",
    scope = "project",
    kind = "list:file",
}
api.register {
    name= "QtIncludes",
    scope = "project",
    kind = "list:file",
}
api.register {
    name= "QtCopyFiles",
    scope = "project",
    kind = "string",
}

p.modules.qt = {}
local qt = p.modules.qt

function qt.solution_pro(sln)
    _p('TEMPLATE = subdirs')
    _p('')
    if #sln.QtSlnConfigs > 0 then
        _p('CONFIG += ' .. table.concat(sln.QtSlnConfigs, " "))
    end
    _p('SUBDIRS += \\')
    for k,v in ipairs(sln.projects) do
        local prj_dir = path.getrelative(sln.location, sln.projects[k].location .. "/" .. sln.projects[k].name)
        if k ~= #sln.projects then
            prj_dir = prj_dir .. ' \\'
        end
        _p(1, prj_dir)
    end
end

function qt.add_includedirs(tab, datatable, prj)
    if #datatable > 0 then
        _p(tab, 'INCLUDEPATH += \\')
        for k,v in ipairs(datatable) do
            local relative_str = path.getrelative(prj.location .. "/" .. prj.name, v)
            if k ~= #datatable then
                relative_str = relative_str .. ' \\'
            end
            _p(tab + 1, relative_str)
        end
        _p('')
    end
end

local links = {}
links["Release"] = {}
links["Debug"] = {}
links["Common"] = {}
function qt.parse_links(prj)
    for _,link in ipairs(prj.links) do
        local lprj = p.workspace.findproject(prj.workspace, link)
        if lprj and lprj ~= nil then
            local release_cfg = project.findClosestMatch(lprj, "Release")
            local debug_cfg   = project.findClosestMatch(lprj, "Debug")
            if release_cfg.linktarget.basename == debug_cfg.linktarget.basename then
                table.insert(links["Common"], release_cfg.linktarget.basename)
            else
                for _,conf in ipairs(prj.configurations) do
                    local prjcfg = project.findClosestMatch(lprj, conf)
                    table.insert(links[conf], prjcfg.linktarget.basename)
                end
            end
        else
            table.insert(links["Common"], link)
        end
    end
end

function qt.add_links(tab, datatable)
    if #datatable > 0 then
        _p(tab, 'LIBS += \\')
        for k,v in ipairs(datatable) do
            if k ~= #datatable then
                v = v .. ' \\'
            end
            _p(tab + 1, '-l' .. v)
        end
        _p('')
    end
end

function qt.add_libdirs(tab, datatable, prj)
    if #datatable > 0 then
        _p(tab, 'LIBS += \\')
        for k,v in ipairs(datatable) do
            local relative_str = path.getrelative(prj.location .. "/" .. prj.name, v)
            if k ~= #datatable then
                relative_str = relative_str .. ' \\'
            end
            _p(tab + 1, '-L' .. relative_str)
        end
        _p('')
    end
end

function qt.add_targetdir(tab, prj)
    if prj.targetdir then
        local relative_str = path.getrelative(prj.location .. "/" .. prj.name, prj.targetdir)
        _p(tab, 'DESTDIR = ' .. relative_str)
    end
end

function qt.get_targetname(prj)
    local outname = prj.project.name
    if prj.targetname then
        outname = prj.targetname
    end
    if prj.targetprefix then
        outname = prj.targetprefix .. outname
    end
    if prj.targetsuffix then
        outname = outname .. prj.targetsuffix
    end
    return outname
end

function qt.remove_item_by_list(full_table, remove_list)
    for i=1, #full_table do
        for k,v in ipairs(full_table) do
            if table.contains(remove_list, v) then
                table.remove(full_table, k)
            end
        end
    end
    return full_table
end

function qt.add_resources(prj)
    _p('<RCC>')
    _p(1, '<qresource prefix="/res">')
    for k,v in ipairs(prj.QtResources) do
        local relative_str = path.getrelative(prj.location .. "/" .. prj.name, v)
        _p(2, '<file>' .. relative_str .. '</file>')
    end
    _p(1, '</qresource>')
    _p('</RCC>')
end

function qt.project_pro(prj)
    -- default module
    if prj.kind == "WindowedApp" then
        _p('QT += core gui')
        _p('')
        _p('greaterThan(QT_MAJOR_VERSION, 4): QT += widgets')
    elseif prj.kind == "StaticLib" or prj.kind == "SharedLib" or prj.kind == "ConsoleApp" then
        _p('QT -= gui')
    end
    _p('')
    if prj.kind == "StaticLib" or prj.kind == "SharedLib" then
        _p('TEMPLATE = lib')
    end
    _p('')

    -- config project
    if #prj.QtModules > 0 then
        _p('QT += ' .. table.concat(prj.QtModules, " "))
    end
    if #prj.QtConfigs > 0 then
        _p('CONFIG += ' .. table.concat(prj.QtConfigs, " "))
    end
    if prj.QtCopyFiles ~= nil then
        _p('CONFIG += file_copies')
    end
    if prj.kind == "ConsoleApp" then
        _p('CONFIG -= app_bundle')
    end
    if #prj.defines > 0 then
        _p('DEFINES += ' .. table.concat(prj.defines, " "))
    end
    if #prj.QtIncludes > 0 then
        for k,v in ipairs(prj.QtIncludes) do
            local relative_str = path.getrelative(prj.location .. "/" .. prj.name, v)
            _p('include(' .. relative_str .. ')')
        end
    end

    -- compiler & linker flags
    if #prj.buildoptions > 0 then
        _p('QMAKE_CFLAGS += ' .. table.concat(prj.buildoptions, " "))
        _p('QMAKE_CXXFLAGS += ' .. table.concat(prj.buildoptions, " "))
    end
    if #prj.linkoptions > 0 then
        _p('QMAKE_LFLAGS += ' .. table.concat(prj.linkoptions, " "))
    end
    _p('')

    -- files
    local header_list = {}
    local source_list = {}
    local ui_list = {}
    for k,v in ipairs(prj.files) do
        local relative_str = path.getrelative(prj.location .. "/" .. prj.name, v)
        if v:endswith(".hpp") or v:endswith(".h") or v:endswith(".inl") then
            table.insert(header_list, relative_str)
        elseif v:endswith(".cpp") or v:endswith(".cc") or v:endswith(".c") or v:endswith(".asm") then
            table.insert(source_list, relative_str)
        elseif v:endswith(".ui") then
            table.insert(ui_list, relative_str)
        end
    end

    if #header_list > 0 then
        _p('HEADERS += \\')
        for k,v in ipairs(header_list) do
            if k ~= #header_list then
                v = v .. ' \\'
            end
            _p(1, v)
        end
        _p('')
    end

    if #source_list > 0 then
        _p('SOURCES += \\')
        for k,v in ipairs(source_list) do
            if k ~= #source_list then
                v = v .. ' \\'
            end
            _p(1, v)
        end
        _p('')
    end

    if #ui_list > 0 then
        _p('FORMS += \\')
        for k,v in ipairs(ui_list) do
            if k ~= #ui_list then
                v = v .. ' \\'
            end
            _p(1, v)
        end
        _p('')
    end

    if #prj.prelinkcommands > 0 then
        _p('QMAKE_PRE_LINK += \\')
        for k,v in ipairs(prj.prelinkcommands) do
            if k ~= #prj.prelinkcommands then
                v = v .. ' &&\\'
            end
            _p(1, v)
        end
        _p('')
    end

    if #prj.postbuildcommands > 0 then
        _p('QMAKE_POST_LINK += \\')
        for k,v in ipairs(prj.postbuildcommands) do
            if k ~= #prj.postbuildcommands then
                v = v .. ' \\'
            end
            _p(1, v)
        end
        _p('')
    end

    -- links
    qt.parse_links(prj)
    if #links["Release"] > 0 or #links["Debug"] > 0 or #links["Common"] > 0 then
        _p('LIBS += -Wl,--start-group')
    end
    qt.add_links(0, links["Common"])
    qt.add_libdirs(0, prj.libdirs, prj)
    qt.add_includedirs(0, prj.includedirs, prj)
    _p('')

    -- debug or release
    local is_one_targetname = true
    local is_one_targetdir = true
    for cfg in project.eachconfig(prj) do
        _p('CONFIG(' .. string.lower(cfg.buildcfg) .. ', debug|release) {')

        if cfg.targetdir ~= prj.project.targetdir then
            is_one_targetdir = false
            qt.add_targetdir(1, cfg)
        end

        if qt.get_targetname(prj) ~= qt.get_targetname(cfg) or is_one_targetname == false then
            is_one_targetname = false
            _p(1, 'TARGET = ' .. qt.get_targetname(cfg))
        end

        local defines = qt.remove_item_by_list(cfg.defines, prj.defines)
        if #defines > 0 then
            _p(1, 'DEFINES += ' .. table.concat(defines, " "))
        end

        qt.add_links(1, links[cfg.buildcfg])

        local libdirs = qt.remove_item_by_list(cfg.libdirs, prj.libdirs)
        qt.add_libdirs(1, libdirs, cfg)

        local includedirs = qt.remove_item_by_list(cfg.includedirs, prj.includedirs)
        qt.add_includedirs(1, includedirs, cfg)
        _p('}')
        _p('')
    end
    if #links["Release"] > 0 or #links["Debug"] > 0 or #links["Common"] > 0 then
        _p('LIBS += -Wl,--end-group')
    end
    if is_one_targetname then
        _p('TARGET = ' .. qt.get_targetname(prj))
    end
    if is_one_targetdir then
        qt.add_targetdir(0, prj)
    end
    _p('')

    -- copy files
    if prj.QtCopyFiles ~= nil then
        _p('COPIES += addfiles')
        _p('addfiles.files = $$files(' .. prj.QtCopyFiles .. ')')
        _p('addfiles.path = $$DESTDIR')
        _p('')
    end

    -- app properties
    if prj.AppName then
        _p('QMAKE_TARGET_PRODUCT = \"' .. prj.AppName .. '\"')
    end
    if prj.AppCompany then
        _p('QMAKE_TARGET_COMPANY = \"' .. prj.AppCompany .. '\"')
    end
    if prj.AppCopyright then
        _p('QMAKE_TARGET_COPYRIGHT = \"' .. prj.AppCopyright .. '\"')
    end
    if prj.AppDescription then
        _p('QMAKE_TARGET_DESCRIPTION = \"' .. prj.AppDescription .. '\"')
    end
    if prj.AppVersion then
        _p('VERSION = \"' .. prj.AppVersion .. '\"')
    end
    if prj.AppIcon then
        local relative_str = path.getrelative(prj.location .. "/" .. prj.name, prj.AppIcon)
        _p('RC_ICONS += ' .. relative_str)
    end
    if #prj.QtResources > 0 then
        local qt_resfile = "resources.qrc"
        p.generate(prj, prj.name .. "/" .. qt_resfile, qt.add_resources)
        _p('RESOURCES += ' .. qt_resfile)
    end
    _p('')

    -- default rules
    _p('# Default rules for deployment.')
    if prj.kind == "WindowedApp" or prj.kind == "ConsoleApp" then
        _p('qnx: target.path = /tmp/$${TARGET}/bin')
        _p('else: unix:!android: target.path = /opt/$${TARGET}/bin')
        _p('!isEmpty(target.path): INSTALLS += target')
    elseif prj.kind == "StaticLib" then
        _p('unix {')
        _p('    target.path = $$[QT_INSTALL_PLUGINS]/generic')
        _p('}')
        _p('!isEmpty(target.path): INSTALLS += target')
    elseif prj.kind == "SharedLib" then
        _p('unix {')
        _p('    target.path = /usr/lib')
        _p('}')
        _p('!isEmpty(target.path): INSTALLS += target')
    end
end

newaction
{
    trigger = "Qt",
    shortname = "Qt",
    description = "Qt qmake project generator",

    valid_kinds = {
        "ConsoleApp",
        "WindowedApp",
        "SharedLib",
        "StaticLib"
    },

    valid_languages = {
        "C",
        "C++"
    },

    valid_tools = {
        cc = {
            "gcc",
            "clang"
        },
    },

    onsolution = function(sln)
        p.generate(sln, sln.name .. ".pro", p.modules.qt.solution_pro)
    end,

    onproject = function(prj)
        p.generate(prj, prj.name .. "/" .. prj.name .. ".pro", p.modules.qt.project_pro)
    end,

    oncleansolution = function(sln)
    end,

    oncleanproject  = function(prj)
    end,
}