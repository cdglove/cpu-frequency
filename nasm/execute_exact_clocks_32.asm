; Copyright 2015 Google Inc. All Rights Reserved.
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

global execute_exact_clocks

section .text
execute_exact_clocks:
        mov ecx, [esp + 4]
start:
        ; Ten dependent adds
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        ; Ten more dependent adds
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        ; Ten more dependent adds
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        ; Ten more dependent adds
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        ; Ten more dependent adds
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax
        add eax, eax

        sub ecx, 1
        jne start
        ret
