# Toward the finish line!

움직이고 회전하는 발판 사이를 뛰어넘어 결승선에 도달하는 **3D 플랫폼 게임**입니다. 발판의 움직임을 읽기 어려울 때는 슬로우 모션으로 점프 타이밍을 잡고, 점프가 조금 모자랄 때는 장애물 모서리를 잡고 올라설 수 있습니다.

[프로젝트 홈페이지](https://kdw1102.imweb.me/Towardthefinishline) · [플레이 영상: 시작부터 결승선까지](https://youtu.be/eG20lvpm8s8) · [게임 빌드 다운로드](https://naver.me/5o0cTBYd)

[![Toward the finish line! 대표 화면 — 클릭하면 플레이 영상으로 이동합니다](https://cdn-optimized.imweb.me/v2/brand/S20260802eaf9fc95f5402/4ffa5eac-b293-4435-aa60-0424c634d7cf)](https://youtu.be/eG20lvpm8s8)

## 프로젝트 개요

| 항목 | 내용 |
| --- | --- |
| 개발 | 김동우 · 개인 프로젝트 |
| 제작 기간 | 2025년 8월 |
| 플랫폼 / 엔진 | PC / Unreal Engine 5.6 |
| 사용 기술 | C++, Blueprint, Enhanced Input, UMG, Animation Montage, Anim Notify |
| 담당 | 플레이어 조작·등반·슬로우 모션, 이동·회전 발판, 장애물 배치, 도착 판정·시간 측정, HUD |

## 조작 방법

| 입력 | 동작 |
| --- | --- |
| W / A / S / D | 이동 |
| Shift | 달리기 |
| Space | 점프 |
| 마우스 오른쪽 버튼 | 누르는 동안 슬로우 모션, 놓으면 해제 |
| 별도 입력 없음 | 공중에서 모서리 잡기 조건을 충족하면 자동으로 올라타기 |

## 핵심 구현

### 1. 점프가 모자랄 때 모서리를 잡고 올라가기

캡슐이 벽에 닿으면 캐릭터가 공중에 있는지, 벽을 향해 접근했는지, 충분한 속도로 닿았는지 확인합니다. 정면 접근 여부는 **캐릭터 전방 벡터와 벽 법선의 반대 방향 간 내적**으로 판단합니다. 옆으로 스친 벽이나 바닥에 가까운 면에서는 등반을 시작하지 않도록 조건을 나눴습니다.

그다음 가슴 높이에서 앞으로 라인 트레이스를 쏴 벽을 찾고, 벽 위에서 아래로 다시 쏴 윗면을 찾습니다. 캐릭터 발과 윗면의 높이 차가 허용 범위 안이면 모서리에 위치와 방향을 맞추고 등반 몽타주를 시작합니다. 등반이 끝나면 보행 상태를 복원하고 캡슐을 바닥에 맞춥니다.

[충돌·접근 각도 검사](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L356-L401) · [벽과 윗면 탐지](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L403-L451) · [등반 애니메이션과 착지 보정](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L514-L629)

### 2. 시간·화면·소리가 함께 변하는 슬로우 모션

우클릭을 누르면 전역 시간 배율을 낮춰 **장애물과 플레이어를 함께** 느리게 합니다. 이때 배경음의 피치를 낮추고 카메라의 채도를 서서히 줄여, 시간 변화가 화면과 소리에서도 느껴지게 했습니다. 버튼을 놓으면 시간 배율과 피치를 복원하고 화면도 원래 색으로 돌아갑니다.

[버튼 누름·해제 바인딩](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L65-L76) · [시간 배율과 BGM 피치](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L295-L354) · [채도 보간](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L255-L270)

### 3. 같은 클래스로 구성한 왕복 발판과 회전 장애물

`AMovingPlatform`은 이동과 회전을 별도 함수로 처리합니다. 왕복 발판이 한 프레임에 끝점을 지나쳤다면 다음 거리 검사에서 위치를 **설정한 끝점**으로 맞춘 뒤 속도 방향을 뒤집습니다. 초과한 위치를 새 출발점으로 삼지 않기 때문에 왕복을 반복해도 경로가 조금씩 밀리지 않습니다. 회전은 초당 회전량에 `DeltaTime`을 곱해 로컬 회전에 더합니다.

[왕복 끝점 보정과 회전](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/MovingPlatform.cpp#L33-L78) · [발판별 이동·회전 설정](Public/MovingPlatform.h)

### 4. 슬로우 모션과 별도로 측정하는 플레이 시간

기록에 사용할 시간을 실제 시간과 게임 시간 중 선택할 수 있게 했습니다. **C++ 기본 설정은 실제 경과 시간**이어서 슬로우 모션을 써도 타이머가 함께 느려지지 않습니다. 캐릭터가 경과 초를 위젯에 전달하면 위젯은 1시간 미만을 `MM:SS`, 그 이상을 `HH:MM:SS`로 표시합니다.

[경과 시간 계산](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/ObstacleAssualtCharacter.cpp#L271-L293) · [UMG 타이머 표시](https://github.com/Gdongu/Towards-the-finish-line-/blob/afbbabbed5fe0779cff47ad2ac6d6eb7c29b8bbb/Private/PlaytimeWidget.cpp#L4-L24)

## 회고

이 프로젝트에서 C++의 충돌 검사 결과를 블루프린트 설정과 애니메이션에 연결해 실제 조작으로 만드는 과정을 익혔습니다. 특히 모서리 잡기는 벽을 찾는 것만으로 끝나지 않았습니다. 캐릭터가 어느 각도로 닿았는지, 발이 윗면과 얼마나 떨어져 있는지, 올라선 뒤 어디에서 보행을 재개할지까지 정해야 하나의 동작이 완성됐습니다.

핵심 조작에 집중한 만큼 UI와 추가 기능에는 시간을 충분히 쓰지 못했습니다. 다음에는 첫 장애물에서 슬로우 모션 조작을 안내하고 활성 상태를 HUD로 보여주는 작업까지 개발 일정에 포함하고 싶습니다.

<details>
<summary>플레이 스크린샷 3장 보기</summary>

![플레이 스크린샷 1](https://github.com/user-attachments/assets/6fddeef6-aa1a-49f4-b150-88218d6fdf9f)

![플레이 스크린샷 2](https://github.com/user-attachments/assets/770c56d8-de50-4362-916a-ae73a5f809c4)

![플레이 스크린샷 3](https://github.com/user-attachments/assets/c4e49312-0e63-48cc-bd92-ab86d05c4e1b)

</details>

## 실행 및 소스 범위

게임은 상단의 빌드 다운로드 링크에서 받을 수 있습니다. [기존 README의 다운로드 링크](https://naver.me/xLs2dNZ6)도 함께 남깁니다. 이 저장소에는 캐릭터·발판·시간 위젯의 [C++ 헤더](Public/)와 [구현 파일](Private/)이 공개되어 있습니다. 프로젝트 설정, 레벨, 블루프린트 및 애니메이션 에셋은 포함되어 있지 않아 저장소만 복제해서 빌드할 수는 없습니다.
