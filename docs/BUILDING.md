# v0.81 소스와 재빌드

일반 사용자는 릴리스의 EXE를 실행하세요. Python·C 컴파일러 설치는 필요하지 않습니다.

## 보존된 검증 소스

`source-kit/`는 검증된 v26 변환기/플레이어 소스 620개 파일을 내용 변경 없이 보존합니다. `package-files.json`의 파일 해시가 기준입니다. 이 폴더에 남아 있는 “내부 검증본” 표기는 당시 봉인 기록이며, 공개 배포 상태와 버전 설명은 이 저장소 루트의 v0.81 README와 릴리스 노트를 따릅니다.

소스 패처를 직접 사용할 때는 `source-kit/README_KO.txt`, `requirements.txt`, `Patch-development.cmd`를 따르세요. 원본 CUE/BIN과 BIOS가 필요합니다. 플레이어와 DLL의 원본 의존 자료 복원/재빌드는 `source-kit/REBUILD-PLAYER_KO.txt`, `source-kit/REBUILD-CONVERTERS_KO.txt`, `source-kit/BUILD-TOOLCHAIN.json`에 있습니다.

## EXE GUI

`apps/windows_patcher/`는 실제 배포 EXE의 GUI·빌드·검증 소스입니다. Python 3.13, NumPy 2.5.1, Pillow 12.3.0, Unicorn 2.1.4, PyInstaller 6.22.3, pefile과 MinGW x64 도구 체인을 사용했습니다.

재현 시 저장소 루트를 작업 루트로 삼고 `source-kit/`를 `work/r22-resources/release-owned-final-source-candidate-v26/패처 폴더/`로 복사하세요. 릴리스 ZIP의 `source/pyinstaller-6.22.3.tar.gz`를 `work/exe-patcher-bootloader/` 아래 풀어 공식 런처를 빌드한 다음 `apps/windows_patcher/README.txt`를 따릅니다. 빌드에 원본 ROM·BIOS는 필요하지 않고, NDS 생성 때 필요합니다.

EXE의 내부 도구 표기는 `1.0 / 한국어 0.81`입니다. 이번 게임 배포 이름은 **v0.81 전체 테스트용**이며, 검증한 EXE 바이트를 다시 빌드하거나 바꾸지 않고 배포합니다.

외부 소스·동적 라이브러리·실행 환경의 라이선스는 `source-kit/licenses/`, 각 `third_party/`, `runtime-licenses/`에 보존됩니다. 저장소 전체를 하나의 라이선스로 바꾸지 않습니다. GUI 래퍼의 MIT 고지는 `apps/windows_patcher/LICENSE.txt`입니다.
