-- TimeOverlay - script d'aide pour OBS
--
-- Rafraichit des sources Texte a partir des fichiers ecrits par TimeOverlay,
-- plus vite que la relecture native d'OBS.
--
-- Corrections par rapport a la version precedente :
--   * le dossier etait construit avec os.getenv("HOME"), variable absente
--     sous Windows : la concatenation avec nil interrompait le chargement du
--     script. Le dossier est maintenant un champ de configuration, avec une
--     valeur par defaut deduite du systeme.
--   * le trim utilisait "\s+$", qui n'est pas un motif Lua valide (il faut
--     "%s"), donc il ne coupait rien.
--   * changer l'intervalle ne reprogrammait pas le minuteur.

obs = obslua

local interval = 100
local base_path = ""
local sources = {}   -- [nom de la source OBS] = chemin du fichier

-- Fichiers produits par TimeOverlay, et libelle affiche dans les proprietes.
local OUTPUTS = {
    { key = "avant_debut",    label = "Avant le debut (decompte)" },
    { key = "heure",          label = "Heure" },
    { key = "date",           label = "Date" },
    { key = "countdown",      label = "Countdown (temps restant)" },
    { key = "countup",        label = "Countup (temps ecoule)" },
    { key = "depassement",    label = "Depassement" },
    { key = "statut",         label = "Statut" },
    { key = "phase",          label = "Phase en cours" },
    { key = "phase_suivante", label = "Phase suivante" },
    { key = "message",        label = "Message de fin" },
    { key = "annonce",        label = "Annonce" },
}

local function default_base_path()
    -- package.config commence par le separateur de chemin du systeme.
    local sep = package.config:sub(1, 1)
    if sep == "\\" then
        local appdata = os.getenv("APPDATA")
        if appdata then
            return appdata .. "\\TimeOverlay\\obs\\"
        end
        return "C:\\TimeOverlay\\obs\\"
    end

    local home = os.getenv("HOME")
    if home then
        return home .. "/.local/share/TimeOverlay/obs/"
    end
    return "/tmp/TimeOverlay/obs/"
end

local function trim(text)
    -- En Lua, les classes de caracteres s'ecrivent avec % et non \.
    return (text:gsub("^%s*(.-)%s*$", "%1"))
end

local function read_file(path)
    local file = io.open(path, "r")
    if not file then return nil end
    local content = file:read("*all")
    file:close()
    return content
end

local function update_sources()
    for source_name, file_path in pairs(sources) do
        local content = read_file(file_path)
        -- Fichier absent : on laisse la source telle quelle plutot que de la
        -- vider. TimeOverlay est peut-etre simplement en train d'ecrire.
        if content then
            local source = obs.obs_get_source_by_name(source_name)
            if source then
                local settings = obs.obs_data_create()
                obs.obs_data_set_string(settings, "text", trim(content))
                obs.obs_source_update(source, settings)
                obs.obs_data_release(settings)
                obs.obs_source_release(source)
            end
        end
    end
end

function script_description()
    return [[<b>TimeOverlay - script d'aide</b><br/><br/>
Rafraichit des sources Texte a partir des fichiers ecrits par TimeOverlay.<br/><br/>
Pour chaque valeur voulue, creez une source Texte dans OBS puis indiquez son
nom exact ci-dessous. Laissez vide ce que vous n'utilisez pas.]]
end

function script_properties()
    local props = obs.obs_properties_create()

    obs.obs_properties_add_path(props, "base_path", "Dossier TimeOverlay",
                                obs.OBS_PATH_DIRECTORY, nil, nil)
    obs.obs_properties_add_int(props, "interval", "Intervalle (ms)", 50, 5000, 50)

    for _, output in ipairs(OUTPUTS) do
        obs.obs_properties_add_text(props, "source_" .. output.key,
                                    output.label, obs.OBS_TEXT_DEFAULT)
    end

    return props
end

function script_defaults(settings)
    obs.obs_data_set_default_string(settings, "base_path", default_base_path())
    obs.obs_data_set_default_int(settings, "interval", 100)
end

function script_update(settings)
    local new_interval = obs.obs_data_get_int(settings, "interval")
    if new_interval < 50 then new_interval = 50 end

    base_path = obs.obs_data_get_string(settings, "base_path")
    if base_path == "" then base_path = default_base_path() end

    -- Garantit un separateur final, que l'utilisateur l'ait saisi ou non.
    local sep = package.config:sub(1, 1)
    if base_path:sub(-1) ~= "/" and base_path:sub(-1) ~= "\\" then
        base_path = base_path .. sep
    end

    sources = {}
    for _, output in ipairs(OUTPUTS) do
        local source_name = obs.obs_data_get_string(settings, "source_" .. output.key)
        if source_name ~= "" then
            sources[source_name] = base_path .. output.key .. ".txt"
        end
    end

    -- Reprogramme le minuteur : sans cela, modifier l'intervalle n'avait
    -- aucun effet jusqu'au rechargement du script.
    if new_interval ~= interval then
        interval = new_interval
        obs.timer_remove(update_sources)
        obs.timer_add(update_sources, interval)
    end
end

function script_load(settings)
    obs.timer_add(update_sources, interval)
end

function script_unload()
    obs.timer_remove(update_sources)
end
