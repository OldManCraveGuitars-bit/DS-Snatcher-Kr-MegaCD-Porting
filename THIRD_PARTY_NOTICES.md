# 외부 코드·자료와 권리 표기

SNATCHER 원작의 권리는 KONAMI 등 해당 권리자에게 있습니다. 비공식 팬 이식이며 공식 제품 또는 보증을 의미하지 않습니다. 소개 스크린샷의 원작 그래픽에 프로젝트 코드의 라이선스를 적용하지 않습니다. 원본 디스크·BIOS·변환된 게임 리소스·완성 NDS는 배포 ZIP에 없습니다.

한국어 대사·보이스 자막은 같은 제작자의 [Mega CD 한국어 패치 v0.775](https://github.com/OldManCraveGuitars-bit/MegaCD-Snatcher-Kr)를 기준으로 합니다. 장식 UI는 이 이식을 위해 제작한 자료입니다.

| 구성 요소 | 고지와 소스 |
| --- | --- |
| PicoDrive/MAME 계열 YM2612 | `third_party/picodrive/COPYING`, `player/source/ym2612*` 및 관련 FM 소스. 비상업적 배포와 수정 바이너리의 관련 전체 소스 제공 조건을 유지합니다. |
| Genesis Plus GX/Musashi | `third_party/GenesisPlusGX/LICENSE.txt`, `third_party/forecourt-driver/LICENSE.txt`. CPU 코어와 수정한 드라이버 어댑터 소스가 포함됩니다. 해당 라이선스의 비상업적 배포 조건을 따릅니다. |
| Nuked-OPN2 | `third_party/Nuked-OPN2/LICENSE`의 LGPL 2.1. 소스 `ym3438.c/.h`와 결합하는 시작 음원 helper의 전체 소스·재빌드 절차를 제공합니다. LGPL 구성 요소의 수정·재링크 권한을 제한하지 않습니다. |
| floooh/chips Z80 | `third_party/chips-z80/z80.h` 안의 zlib 라이선스와 저작권 고지를 유지합니다. |
| BlocksDS/libnds, FatFs | `third_party/BlocksDS-libnds/LICENSE`, `LICENSE.FatFs`. 저장 호환 선언과 변경 내용은 `player/include/storage_compat/`에 있습니다. |
| Maxmod | `third_party/maxmod/COPYING`, 이식의 ARM7/오디오 관련 변경 소스. SDK의 libmm9/libmm7과 연결합니다. |
| Galmuri | `third_party/Galmuri-OFL.txt`의 SIL OFL 1.1. 게임에 포함된 한국어 글리프는 이 글꼴에서 생성했습니다. |
| Python 패키지 | NumPy, Pillow, Unicorn은 requirements.txt로 별도 설치하며 각 프로젝트의 라이선스를 따릅니다. 패처 ZIP에 Python 실행 환경을 포함하지 않습니다. |

이 저장소 전체에 단일 MIT/GPL 라이선스를 적용하지 않습니다. 외부 구성 요소에는 원래 라이선스와 저작권 고지가 각각 적용됩니다. 원본 게임 자료의 권리와 이식 코드·번역·장식 UI의 권리를 혼동하지 마십시오. 소스와 빌드 방법은 [개발자 안내](docs/BUILDING.md)를 참조하십시오.
