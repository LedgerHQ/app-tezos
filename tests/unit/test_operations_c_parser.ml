(* Copyright 2023 Functori <contact@functori.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. *)

open Tezos_protocol_016_PtMumbai
open Test_c_parser_utils

let pp_opt_field pp ppf = function
  | None -> Format.fprintf ppf "Field unset"
  | Some v -> Format.fprintf ppf "%a" pp v

let pp_tz ppf tz = Format.fprintf ppf "%a tz" Protocol.Alpha_context.Tez.pp tz

let to_string
    ( (_shell : Tezos_base.Operation.shell_header),
      (Contents_list contents : Protocol.Alpha_context.packed_contents_list) ) =
  let open Protocol.Alpha_context in
  let manager_to_string (type t)
      (Manager_operation { fee; operation; storage_limit; _ } :
        t Kind.manager contents) =
    let aux ~kind operation_fields =
      let manager_fields =
        [ kind; Format.asprintf "%a tz" Tez.pp fee; Z.to_string storage_limit ]
      in
      String.concat "" (manager_fields @ operation_fields)
    in
    match operation with
    | Delegation public_key_hash_opt ->
        aux ~kind:"Delegation"
          [
            Format.asprintf "%a"
              (pp_opt_field Tezos_crypto.Signature.Public_key_hash.pp)
              public_key_hash_opt;
          ]
    | Reveal public_key ->
        aux ~kind:"Reveal"
          [
            Format.asprintf "%a" Tezos_crypto.Signature.Public_key.pp public_key;
          ]
    | Set_deposits_limit tez_opt ->
        aux ~kind:"Set deposit limit"
          [ Format.asprintf "%a" (pp_opt_field pp_tz) tez_opt ]
    | Transaction { amount; entrypoint; destination; parameters } ->
        let parameters =
          Result.get_ok @@ Protocol.Script_repr.force_decode parameters
        in
        aux ~kind:"Transaction"
          [
            Format.asprintf "%a tz" Tez.pp amount;
            Format.asprintf "%a" Contract.pp destination;
            Format.asprintf "%a" Entrypoint.pp entrypoint;
            Test_micheline_c_parser.to_string parameters;
          ]
    | _ -> assert false
  in
  let rec operations_to_string : type t. t contents_list -> string = function
    | Single (Manager_operation _ as m) -> manager_to_string m
    | Cons ((Manager_operation _ as m), rest) ->
        manager_to_string m ^ operations_to_string rest
    | _ -> assert false
  in
  operations_to_string contents

let to_bytes op =
  Data_encoding.Binary.to_bytes_exn
    Protocol.Alpha_context.Operation.unsigned_encoding op

external cparse_step :
  cparse_state ->
  input:bytes * int * int ->
  output:bytes * int * int ->
  int * int * st = "operation_cparse_step"
