# MakineAI Hook

> Oyun cevirisi icin super arac seti -- hooking, asset analizi, bellek tarama

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Platform: win64](https://img.shields.io/badge/Platform-win64-lightgrey.svg)]()
[![API Version: 1](https://img.shields.io/badge/Plugin%20API-v1-green.svg)]()

## Nedir?

MakineAI Hook, [MakineAI Launcher](https://github.com/MakineCeviri/MakineAI-Launcher) icin gelistirilen topluluk eklentisidir. Oyun metinlerini yakalamak, oyun dosyalarini analiz etmek ve bellekten metin cikarmak icin gereken tum araclari tek bir eklentide toplar. Iki DLL mimarisi ile calisir: plugin DLL (Launcher tarafindan yuklenir) ve hook DLL (oyun process'ine enjekte edilir).

## Ozellikler

### Hooking

- **10 GDI/User32 Hook** -- TextOutW/A, ExtTextOutW/A, DrawTextW/A, DrawTextExW/A, GetGlyphOutlineW/A
- **x64 Inline Hooking** -- 14-byte JMP ile guvenli fonksiyon yakalama + trampoline
- **Named Pipe IPC** -- `\\.\pipe\MakineAI_TextHook_{PID}` uzerinden dusuk gecikmeli iletisim
- **Tekrar Filtresi** -- Ring buffer (10 girdi) ile ayni metni tekrar gondermez
- **Glyph Biriktirme** -- GetGlyphOutline cagrilarini 50ms timeout ile birlestirerek tam kelime olusturur
- **DLL Injection** -- CreateRemoteThread + LoadLibraryW ile hedef process'e enjeksiyon

### Asset Parsing

- **Unity** -- `.bundle` / `.assets` (AssetBundle header, type tree, string table)
- **Unreal Engine** -- `.pak` / `.locres` (PAK v4 arsiv, localization resource)
- **Bethesda** -- `.ba2` / `.strings` / `.dlstrings` / `.ilstrings` (BA2 arsiv, string tablosu)
- **GameMaker** -- `data.win` / `data.ios` / `data.droid` (FORM chunk, string pool)
- **Ren'Py** -- `.rpa` v2/v3 (RPA arsiv, Python pickle index)

### Bellek Tarama

- **Turkce Karakter Parmak Izi** -- UTF-8/UTF-16 Turkce karakter kaliplariyla bellek taramasi
- **Process Baglama** -- Calisan oyun process'ine baglanarak bellek bolgeleri okuma
- **Encoding Tespiti** -- Sifrelenmis/obfuscate edilmis metinleri tespit etme

## Mimari

Iki DLL mimarisi:

```
MakineAI Launcher
    | LoadLibrary
makineaihook.dll (Plugin DLL -- ~3 MB)
    |-- Asset Parser    -> Oyun dosyalarini analiz et
    |-- Memory Scanner  -> Bellekten metin cikar
    '-- Hook Manager    -> Oyun process'ine enjekte et
         | CreateRemoteThread
         makineai-hook.dll (Hook DLL -- ~156 KB)
              | Inline Hook (14-byte JMP)
              TextOutW, DrawTextW, ExtTextOutW...
              | Named Pipe
              makineaihook.dll <- Yakalanan metin
```

| DLL | Boyut | Gorev |
|-----|-------|-------|
| `makineaihook.dll` | ~3 MB | Plugin -- asset parsing, bellek tarama, injection yonetimi, pipe sunucu |
| `makineai-hook.dll` | ~156 KB | Hook -- oyuna enjekte edilir, GDI fonksiyonlarini yakalar |

## Desteklenen Motorlar

| Motor | Dosya Formatlari | Okuma | Yazma |
|-------|-----------------|-------|-------|
| Unity | `.bundle`, `.assets` | + | -- |
| Unreal Engine | `.pak`, `.locres` | + | + |
| Bethesda | `.ba2`, `.strings`, `.dlstrings`, `.ilstrings` | + | + |
| GameMaker | `data.win`, `data.ios`, `data.droid` | + | -- |
| Ren'Py | `.rpa` (v2/v3) | + | -- |

## Kurulum

### MakineAI Launcher uzerinden (onerilen)

1. **Eklentiler** sayfasina gidin
2. **MakineAI Hook** bulun ve **Kur** tusuna basin
3. Eklentiyi etkinlestirin

### Elle kurulum

1. [Releases](https://github.com/MakineCeviri/MakineAI-Plugin-MakineAIHook/releases) sayfasindan `.makine` dosyasini indirin
2. Launcher'da **Eklentiler** > **Dosyadan Kur** ile yukleyin

## Derleme

### Gereksinimler

- CMake 3.25+
- MinGW GCC 13.1+ veya MSVC 2022
- vcpkg (zlib, lz4, nlohmann-json)
- Windows 10/11 SDK
- Ninja (onerilen)

### Adimlar

```bash
# vcpkg bagimliliklari kur
vcpkg install zlib:x64-mingw-dynamic lz4:x64-mingw-dynamic nlohmann-json:x64-mingw-dynamic

# Derleme
cmake -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build
```

Iki DLL uretilir: `build/release/makineaihook.dll` (plugin) ve `build/release/makineai-hook.dll` (hook).

### Paketleme

```bash
python makine-pack.py build/release -o makineaihook.makine
```

## C ABI Referansi

Tum exportlar `extern "C" __declspec(dllexport)` ile tanimlanmistir.

### Zorunlu (5)

| Export | Donus | Aciklama |
|--------|-------|----------|
| `makineai_get_info()` | `MakineAiPluginInfo` | Plugin kimlik bilgileri (id, name, version, apiVersion) |
| `makineai_initialize(const char* dataPath)` | `MakineAiError` | Baslat: ayarlari yukle, hook manager ve parser'lari kaydet |
| `makineai_shutdown()` | `void` | Kapat: hook'lari serbest birak, ayarlari kaydet |
| `makineai_is_ready()` | `bool` | Plugin hazir mi? |
| `makineai_get_last_error()` | `const char*` | Son hata mesaji |

### Ayarlar (2)

| Export | Donus | Aciklama |
|--------|-------|----------|
| `makineai_get_setting(const char* key)` | `const char*` | Ayar degerini oku |
| `makineai_set_setting(const char* key, const char* value)` | `void` | Ayar yaz ve kaydet |

### Hooking (4)

| Export | Donus | Aciklama |
|--------|-------|----------|
| `makineai_inject_process(DWORD pid)` | `bool` | Hedef process'e hook DLL enjekte et |
| `makineai_detach_process()` | `void` | Enjekte edilen hook'u kaldir |
| `makineai_get_hooked_text()` | `const char*` | Son yakalanan metni al |
| `makineai_is_injected()` | `bool` | Hook aktif mi? |

### Asset Parsing (4)

| Export | Donus | Aciklama |
|--------|-------|----------|
| `makineai_detect_engine(const char* gamePath)` | `const char*` | Dosyadan oyun motorunu tespit et |
| `makineai_parse_assets(const char* filePath)` | `int` | Dosyayi analiz et, string sayisini dondur (-1 = hata) |
| `makineai_get_string_count()` | `int` | Son parse islemindeki string sayisi |
| `makineai_get_string_at(int index)` | `const char*` | String al (JSON: key, original, context, offset, maxLength) |

### Bellek Tarama (2)

| Export | Donus | Aciklama |
|--------|-------|----------|
| `makineai_scan_memory(DWORD pid)` | `int` | Process bellegini tara, bulunan metin sayisini dondur |
| `makineai_get_scanned_text(int index)` | `const char*` | Taranan metni al (JSON: text, category, encoding, address, length) |

## Ayarlar

`manifest.json` dosyasinda tanimli ayarlar:

| Anahtar | Tur | Varsayilan | Aciklama |
|---------|-----|------------|----------|
| `hookEnabled` | Toggle | `false` | Hook aktif/pasif |
| `targetProcess` | Text | *(bos)* | Hedef oyun process adi (orn: `game.exe`) |
| `hookFilter` | Select | `all` | Hangi hook'lar aktif: `all`, `textout`, `drawtext`, `glyph` |
| `minTextLength` | Select | `2` | Minimum metin uzunlugu (1, 2, 3, 5, 10) |
| `deduplication` | Toggle | `true` | Tekrarlayan metinleri filtrele |
| `memoryScanning` | Toggle | `false` | Bellek tarama aktif/pasif |

## Proje Yapisi

```
MakineAI-Plugin-MakineAIHook/
|-- manifest.json                         Eklenti meta verileri ve ayar tanimlari
|-- CMakeLists.txt                        Iki DLL icin derleme yapilandirmasi
|-- vcpkg.json                            vcpkg bagimliliklari
|-- makine-pack.py                        .makine paketleme araci
|-- include/
|   '-- makineai/
|       |-- plugin/
|       |   |-- plugin_api.h              C ABI tanimlari
|       |   '-- plugin_types.h            Plugin tipleri (MakineAiPluginInfo, MakineAiError)
|       |-- asset_parser.hpp              Asset parser arayuzu + registry
|       |-- parsers_factory.hpp           Parser factory fonksiyonlari
|       |-- memory_extractor.hpp          Bellek tarama arayuzu
|       |-- handlers/
|       |   '-- engine_handler.hpp        IEngineHandler arayuzu (gelecek)
|       |-- types.hpp                     Ortak tipler (StringEntry, TranslationEntry)
|       |-- error.hpp                     Hata tanimlari
|       |-- logging.hpp                   Loglama makrolari
|       '-- metrics.hpp                   Performans metrikleri
|-- src/
|   |-- plugin.cpp                        C ABI giris noktasi (17 export)
|   |-- asset_parser.cpp                  Parser registry ve dispatch
|   |-- settings.h                        Ayar kaliciligi
|   |-- hooking/
|   |   |-- hook_manager.h                Process injection + pipe sunucu
|   |   '-- hook_manager.cpp
|   |-- memory/
|   |   '-- memory_extractor.cpp          Turkce parmak izi bellek taramasi
|   '-- parsers/
|       |-- unity_bundle_parser.cpp       Unity AssetBundle parser
|       |-- unreal_pak_parser.cpp         Unreal PAK/locres parser
|       |-- bethesda_ba2_parser.cpp       Bethesda BA2/strings parser
|       |-- gamemaker_data_parser.cpp     GameMaker data.win parser
|       '-- formats/
|           |-- unity_bundle.hpp          Unity format header tanimlari
|           |-- unreal_pak.hpp            Unreal format header tanimlari
|           |-- bethesda_ba2.hpp          Bethesda format header tanimlari
|           |-- gamemaker_data.hpp        GameMaker format header tanimlari
|           |-- renpy_rpa.hpp             Ren'Py RPA format header tanimlari
|           |-- rpa_archive.cpp           RPA arsiv okuyucu
|           |-- pickle_reader.hpp         Python pickle deserializer
|           '-- pickle_reader.cpp
|-- hook/                                 Hook DLL kaynaklari (oyuna enjekte edilir)
|   |-- dllmain.cpp                       DLL giris noktasi + hook kurulumu
|   |-- text_hooks.h/cpp                  GDI/User32 metin fonksiyon hook'lari
|   '-- pipe_client.h/cpp                 Named pipe istemci
'-- tests/                                Birim testleri
```

## Yol Haritasi

- [x] GDI text hooking (10 fonksiyon)
- [x] x64 inline hooking + trampoline
- [x] Named pipe IPC
- [x] Glyph biriktirme (50ms timeout)
- [x] Ring buffer tekrar filtresi
- [x] Process injection / ejection
- [x] Asset parsers (Unity, Unreal, Bethesda, GameMaker)
- [x] Ren'Py RPA arsiv okuyucu
- [x] Process bellek taramasi (Turkce parmak izi)
- [ ] Engine-specific hooks (Unity IL2CPP, RPG Maker, Ren'Py)
- [ ] Hardware breakpoint hooks (VEH)
- [ ] H-code destegi (Textractor uyumlu)
- [ ] Otomatik process algilama
- [ ] RPG Maker MV/MZ parser

## Uyarilar

- **Yonetici yetkileri** gerekebilir (process enjeksiyonu icin)
- **Anti-cheat** sistemleri (EAC, BattlEye, Vanguard) ile uyumsuz olabilir
- **Antivirus** yazilimlari DLL enjeksiyonunu supheli bulabilir -- false positive olarak isaretleyebilir. Gerekirse istisna ekleyin.

## Katki

Katkilerinizi bekliyoruz! Ozellikle yeni oyun motoru handler'lari ve parser'lar cok degerli.

1. Fork edin
2. Feature branch olusturun (`feat/rpgmaker-parser`)
3. Degisikliklerinizi commit edin
4. Pull Request acin

Yeni eklenti gelistirmek icin [MakineAI-Plugin-Template](https://github.com/MakineCeviri/MakineAI-Plugin-Template) deposunu kullanabilirsiniz.

## Lisans

[GPL-3.0](LICENSE)
