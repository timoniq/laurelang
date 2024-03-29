𝔹 ~ @{false, true}.

: ? ∧ (𝔹, 𝔹) -> 𝔹,
  ? ∨ (𝔹, 𝔹) -> 𝔹,
  ? ¬ (𝔹) -> 𝔹,
  ? ∅ ().

? ∧ (@true, @true) -> @true.
? ∧ (@false, _) -> @false.
? ∧ (_, @false) -> @false.

? ∨ (@true, _) -> @true.
? ∨ (_, @true) -> @true.
? ∨ (@false, @false) -> @false.

?¬(@false) -> @true.
?¬(@true) -> @false.

? ∅ () when fail().

-- DeMorgan laws
: ?DeMorgan(𝔹, 𝔹).
-- DeMorgan₁
?DeMorgan(a, b) {
    ¬(a ∨ b) = ¬(a) ∧ ¬(b);
}
-- DeMorgan₂
?DeMorgan(a, b) {
    ¬(a ∧ b) = ¬(a) ∨ ¬(b);
}

: ?must_be_true(𝔹).
?must_be_true(@true).

AND ~ ∧.
OR ~ ∨.
NOT ~ ¬.

± ~ +-.