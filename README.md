# MakineAI TextHook

> Oyun belleğinden doğrudan metin çıkarma eklentisi — MakineAI Launcher için.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)

## Özellikler

- **GDI Text Hooking** — TextOutW, DrawTextW, ExtTextOutW, GetGlyphOutlineW ve ANSI varyantları
- **x64 Inline Hooking** — 14-byte JMP ile güvenli fonksiyon yakalama + trampoline
- **DLL Injection** — CreateRemoteThread + LoadLibraryW ile hedef process'e enjeksiyon
- **Named Pipe IPC** — Oyun process'i ile launcher arasında düşük gecikmeli iletişim
- **Glyph Biriktirme** — GetGlyphOutline çağrılarını 50ms timeout ile birleştirir
- **Tekrar Filtresi** — Ring buffer (10 girdi) ile aynı metni tekrar göndermez
- **Ayarlanabilir** — Hedef process, hook filtresi, minimum metin uzunluğu

## Mimari

İki DLL mimarisi:

```
MakineAI Launcher
    ↓ LoadLibrary
makineai-texthook.dll (Plugin DLL)
    ↓ CreateRemoteThread + LoadLibraryW
    ↓ (DLL Injection)
Oyun Process'i
    ↓
makineai-hook.dll (Hook DLL)
    ↓ Inline Hook (14-byte JMP)
    ↓ TextOutW, DrawTextW, ExtTextOutW...
    ↓ Yakalanan metin
    ↓ Named Pipe (\\.\pipe\MakineAI_TextHook_{PID})
    ↓
makineai-texthook.dll (Pipe Server)
    ↓
Launcher → Çeviri Pipeline
```

| DLL | Boyut | Görev |
|-----|-------|-------|
| `makineai-texthook.dll` | ~358 KB | Plugin — injection yönetimi, pipe sunucu, metin toplama |
| `makineai-hook.dll` | ~157 KB | Hook — oyuna enjekte edilir, GDI fonksiyonlarını yakalar |

## Hook Edilen Fonksiyonlar

| Fonksiyon | Kütüphane | Açıklama |
|-----------|-----------|----------|
| `TextOutW` / `TextOutA` | gdi32 | Temel metin çıktısı |
| `ExtTextOutW` / `ExtTextOutA` | gdi32 | Genişletilmiş metin çıktısı |
| `DrawTextW` / `DrawTextA` | user32 | Dikdörtgen içinde metin |
| `DrawTextExW` / `DrawTextExA` | user32 | Genişletilmiş dikdörtgen metin |
| `GetGlyphOutlineW` / `GetGlyphOutlineA` | gdi32 | Glyph bazlı metin (glyph biriktirme ile) |

## Kurulum

### MakineAI Launcher üzerinden (önerilen)
1. Ayarlar → Eklentiler → **MakineAI TextHook** → **Kur**
2. Eklentiyi etkinleştir
3. Hedef oyun process adını girin
4. Hook'u başlatın

### Elle kurulum
1. [Releases](https://github.com/MakineCeviri/MakineAI-Plugin-TextHook/releases) sayfasından `.makine` dosyasını indirin
2. Launcher → Eklentiler → **Dosya Seç** ile yükleyin

## Ayarlar

| Ayar | Tür | Varsayılan | Açıklama |
|------|-----|------------|----------|
| Hook Aktif | Toggle | Kapalı | Ana açma/kapama |
| Hedef İşlem | Metin | — | Oyun exe adı (ör: game.exe) |
| Hook Filtresi | Seçim | Tümü | all, textout, drawtext, glyph |
| Min Metin Uzunluğu | Seçim | 2 | 1-10 karakter minimum |
| Tekrar Filtresi | Toggle | Açık | Aynı metni tekrar gönderme |

## Uyarılar

- **Yönetici yetkileri** gerekebilir (process enjeksiyonu için)
- **Anti-cheat** sistemleri (EAC, BattlEye) ile uyumsuz olabilir
- **Antivirüs** yazılımları DLL enjeksiyonunu şüpheli bulabilir — false positive

## Geliştirme

### Gereksinimler
- CMake 3.25+
- C++23 derleyici (MSVC 2022 veya MinGW GCC 13.1+)
- Windows 10/11 SDK

### Derleme
```bash
mkdir build && cd build
cmake .. -G Ninja
cmake --build .
```

İki DLL üretilir: `makineai-texthook.dll` (plugin) ve `makineai-hook.dll` (injected).

### Paketleme
Her iki DLL de `.makine` paketine dahil edilmelidir:
```bash
python makine-pack.py build/release -o makineai-texthook.makine
```

`makine-pack.py` aracını [MakineAI-Plugin-Template](https://github.com/MakineCeviri/MakineAI-Plugin-Template) deposundan edinebilirsiniz.

## Proje Yapısı

```
├── manifest.json              — Eklenti meta verileri ve ayar tanımları
├── CMakeLists.txt             — Her iki DLL için derleme yapılandırması
├── src/                       — Plugin DLL kaynakları
│   ├── plugin.cpp             — C ABI giriş noktası
│   ├── hook_manager.h/cpp     — Process injection + pipe sunucu
│   └── settings.h             — Ayar kalıcılığı
├── hook/                      — Hook DLL kaynakları (oyuna enjekte edilir)
│   ├── dllmain.cpp            — DLL giriş noktası + hook kurulumu
│   ├── text_hooks.h/cpp       — GDI/User32 metin fonksiyon hook'ları
│   └── pipe_client.h/cpp      — Named pipe istemci
└── include/
    └── makineai/plugin/       — Plugin SDK başlıkları
```

## Yol Haritası

- [x] x64 inline hooking (14-byte JMP + trampoline)
- [x] GDI text function hooks (10 fonksiyon)
- [x] Named pipe IPC
- [x] Glyph biriktirme (50ms timeout)
- [x] Ring buffer tekrar filtresi
- [x] Process injection/ejection
- [ ] Engine-specific hooks (Unity IL2CPP, RPG Maker, Ren'Py)
- [ ] Hardware breakpoint hooks (VEH)
- [ ] H-code desteği (Textractor uyumlu)
- [ ] Otomatik process algılama

## Katkıda Bulunma

Katkılarınızı bekliyoruz! Özellikle yeni oyun motoru handler'ları çok değerli.

1. Fork edin
2. Feature branch oluşturun (`feat/unity-handler`)
3. Değişikliklerinizi commit edin
4. Pull Request açın

## Lisans

[GPL-3.0](LICENSE)
